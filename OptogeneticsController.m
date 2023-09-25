%% OptogeneticsController: construct graphical user interface to interact with arduino
% 
%   Last Modified: ahamilos 9-24-23 
% 
% Update log:
%	9-24-23: Incorporating task scheduler
%   3-16-18: (from OptogeneticsController 11/2/17 build, AH) 
% 
% 

classdef OptogeneticsController < handle
	properties
		Arduino
		Nidaq
		UserData
	end
	properties (Transient)
		Rsc
	end

	%----------------------------------------------------
	%		Methods
	%----------------------------------------------------
	methods
		function obj = OptogeneticsController()
			% Find arduino port
			arduinoPortName = OptogeneticsController.QueryPort();

			% Splash
			if ~strcmp(arduinoPortName, '/offline')
				obj.CreateDialog_Splash()
			end

			% Establish arduino connection
			obj.Arduino = ArduinoConnection(arduinoPortName);

			% Creata Experiment Control window with all the knobs and buttons you need to set up an experiment. 
			obj.CreateDialog_ExperimentControl()

			% Create Monitor window with all thr trial results and plots and stuff so the Grad Student is ON TOP OF THE SITUATION AT ALL TIMES.
			obj.CreateDialog_Monitor()

			% 9-23-23: adding task scheduler, comment out to remove as default
			% Create Task Scheduler
			obj.CreateDialog_TaskScheduler()
			if isfield(obj.Rsc, 'TaskScheduler') && isvalid(obj.Rsc.TaskScheduler)
				position = obj.Rsc.TaskScheduler.OuterPosition(1:2) + [0, obj.Rsc.TaskScheduler.OuterPosition(4)];
			else
				position = [];
			end

			% Kill splash
			if ~strcmp(arduinoPortName, '/offline')
				obj.CloseDialog_Splash()
			end

			% This gives the Optocontroller handle to Arduino Connection
			obj.Arduino.OptogeneticsController = obj;
		end

		function CreateDialog_ExperimentControl(obj)
			% If object already exists, show window
			if isfield(obj.Rsc, 'ExperimentControl')
				if isvalid(obj.Rsc.ExperimentControl)
					figure(obj.Rsc.ExperimentControl)
					return
				end
			end

			% Size and position of controls
			buttonWidth = 50; % Width of buttons
			buttonHeight = 20; % Height of 
			ctrlSpacing = 10; % Spacing between ui elements

			% Create the dialog
			if isempty(obj.Arduino.SerialConnection)
				port = 'OFFLINE';
			else
				port = obj.Arduino.SerialConnection.Port;
			end
			dlg = dialog(...
				'Name', sprintf('Optogenetics Control (%s)', port),...
				'WindowStyle', 'normal',...
				'Resize', 'on',...
				'Visible', 'off'... % Hide until all controls created
			);
			% Store the dialog handle
			obj.Rsc.ExperimentControl = dlg;

			% Close serial port when you close the window
			dlg.CloseRequestFcn = {@OptogeneticsController.ArduinoClose, obj};

			% Create a uitable for parameters
			table_params = uitable(...
				'Parent', dlg,...
				'Data', obj.Arduino.ParamValues',...
				'RowName', obj.Arduino.ParamNames,...
				'ColumnName', {'Value'},...
				'ColumnFormat', {'long'},...
				'ColumnEditable', [true],...
				'CellEditCallback', {@OptogeneticsController.OnParamChangedViaGUI, obj.Arduino}...
			);
			dlg.UserData.Ctrl.Table_Params = table_params;

			% 9-23-23: Add listener for parameter change via non-GUI methods, in which case we'll update table_params
			if ~isfield(obj.Arduino.Listeners, 'ParamChanged') || ~isvalid(obj.Arduino.Listeners.ParamChanged)
				obj.Arduino.Listeners.ParamChanged = addlistener(obj.Arduino, 'ParamValues', 'PostSet', @obj.OnParamChanged);
			end
			% % Add listener for parameter change via non-GUI methods, in which case we'll update table_params
			% obj.Arduino.Listeners.ParamChanged = addlistener(obj.Arduino, 'ParamValues', 'PostSet', @obj.OnParamChanged);

			% Set width and height
			table_params.Position(3:4) = table_params.Extent(3:4);

			% Start button - start experiment from IDLE
			ctrlPosBase = table_params.Position;
			ctrlPos = [...
				ctrlPosBase(1) + ctrlPosBase(3) + ctrlSpacing,...
				ctrlPosBase(2) + ctrlPosBase(4) - buttonHeight - ctrlSpacing,...
				buttonWidth,...	
				buttonHeight...
			];
			button_start = uicontrol(...
				'Parent', dlg,...
				'Style', 'pushbutton',...
				'Position', ctrlPos,...
				'String', 'Start',...
				'TooltipString', 'Tell Arduino to start the experiment. Breaks out of IDLE state.',...
				'Callback', {@OptogeneticsController.ArduinoStart, obj.Arduino}...
			);

			% Stop button - abort current trial and return to IDLE
			ctrlPosBase = button_start.Position;
			ctrlPos = [...
				ctrlPosBase(1),...
				ctrlPosBase(2) - buttonHeight - ctrlSpacing,...
				buttonWidth,...	
				buttonHeight...
			];
			button_stop = uicontrol(...
				'Parent', dlg,...
				'Style', 'pushbutton',...
				'Position', ctrlPos,...
				'String', 'Stop',...
				'TooltipString', 'Puts Arduino into IDLE state. Resume using the "Start" button.',...
				'Callback', {@OptogeneticsController.ArduinoStop, obj.Arduino}...
			);

			% Reset button - software reset for arduino
			ctrlPosBase = button_stop.Position;
			ctrlPos = [...
				ctrlPosBase(1),...
				ctrlPosBase(2) - buttonHeight - ctrlSpacing,...
				buttonWidth,...	
				buttonHeight...
			];
			button_reset = uicontrol(...
				'Parent', dlg,...
				'Style', 'pushbutton',...
				'Position', ctrlPos,...
				'String', 'Reset',...
				'TooltipString', 'Reset Arduino (parameters will not be changed).',...
				'Callback', {@OptogeneticsController.ArduinoReset, obj.Arduino}...
			);
%% AH 3/16/18 update --------------------------------------------------------------------------
			% Stimulate ChR2 button - for use with Optogenetics Controller
			ctrlPosBase = button_reset.Position;
			ctrlPos = [...
				ctrlPosBase(1),...
				ctrlPosBase(2) - buttonHeight - ctrlSpacing,...
				buttonWidth,...	
				buttonHeight...
			];
			button_UI_ChR2 = uicontrol(...
				'Parent', dlg,...
				'Style', 'pushbutton',...
				'Position', ctrlPos,...
				'String', 'ChR2',...
				'TooltipString', 'Stimulate ChR2 with UI parameters.',...
				'Callback', {@OptogeneticsController.StimulateChR2, obj.Arduino}...
			);
%% --------------------------------------------------------------------------------------------
%% AH 5/2/18 update --------------------------------------------------------------------------
			% Optotag ChR2 button - for use with Optogenetics Controller
			ctrlPosBase = button_UI_ChR2.Position;
			ctrlPos = [...
				ctrlPosBase(1),...
				ctrlPosBase(2) - buttonHeight - ctrlSpacing,...
				buttonWidth,...	
				buttonHeight...
			];
			button_Optotag = uicontrol(...
				'Parent', dlg,...
				'Style', 'pushbutton',...
				'Position', ctrlPos,...
				'String', 'Optotag',...
				'TooltipString', 'Run Optotagging Ladder.',...
				'Callback', {@OptogeneticsController.Optotag, obj.Arduino}...
			);
%% --------------------------------------------------------------------------------------------



			% Resize dialog so it fits all controls
			screen_size = get(0,'ScreenSize');
			%  x y width height
			dlg.Position(3) = table_params.Position(3) + buttonWidth + 4*ctrlSpacing;
			dlg.Position(4) = table_params.Position(4) + 3*ctrlSpacing;
			dlg.Position(1:2) = [screen_size(3)-dlg.Position(3)-10, 50];

			% Menus
			menu_file = uimenu(dlg, 'Label', '&File');
			uimenu(menu_file, 'Label', 'Save Experiment ...', 'Callback', {@OptogeneticsController.ArduinoSaveExperiment, obj.Arduino}, 'Accelerator', 's');
			uimenu(menu_file, 'Label', 'Save Experiment As ...', 'Callback', {@OptogeneticsController.ArduinoSaveAsExperiment, obj.Arduino});
			uimenu(menu_file, 'Label', 'Load Experiment ...', 'Callback', {@OptogeneticsController.ArduinoLoadExperiment, obj.Arduino}, 'Accelerator', 'l');
			uimenu(menu_file, 'Label', 'Save Parameters ...', 'Callback', {@OptogeneticsController.ArduinoSaveParameters, obj.Arduino}, 'Separator', 'on');
			uimenu(menu_file, 'Label', 'Load Parameters ...', 'Callback', {@OptogeneticsController.ArduinoLoadParameters, obj.Arduino, table_params});
			uimenu(menu_file, 'Label', 'Quit', 'Callback', {@OptogeneticsController.ArduinoClose, obj}, 'Separator', 'on');

			menu_arduino = uimenu(dlg, 'Label', '&Arduino');
			uimenu(menu_arduino, 'Label', 'Start', 'Callback', {@OptogeneticsController.ArduinoStart, obj.Arduino});
			uimenu(menu_arduino, 'Label', 'Stop', 'Callback', {@OptogeneticsController.ArduinoStop, obj.Arduino}, 'Accelerator', 'q');
			uimenu(menu_arduino, 'Label', 'Reset', 'Callback', {@OptogeneticsController.ArduinoReset, obj.Arduino}, 'Separator', 'on');
			uimenu(menu_arduino, 'Label', 'Reconnect', 'Callback', {@OptogeneticsController.ArduinoReconnect, obj.Arduino});

			menu_window = uimenu(dlg, 'Label', '&Window');
			uimenu(menu_window, 'Label', 'Experiment Control', 'Callback', @(~, ~) @obj.CreateDialog_ExperimentControl);
			uimenu(menu_window, 'Label', 'Monitor', 'Callback', @(~, ~) obj.CreateDialog_Monitor);

			% Unhide dialog now that all controls have been created
			dlg.Visible = 'on';
		end

		function CreateDialog_Monitor(obj)
			% If object already exists, show window
			if isfield(obj.Rsc, 'Monitor')
				if isvalid(obj.Rsc.Monitor)
					figure(obj.Rsc.Monitor)
					return
				end
			end
			dlgWidth = 800;
			dlgHeight = 400;

			% Create the dialog
			if isempty(obj.Arduino.SerialConnection)
				port = 'OFFLINE';
			else
				port = obj.Arduino.SerialConnection.Port;
			end

			% Resize dialog so it fits all controls
			screen_size = get(0, 'ScreenSize');
			%  x y width height
			% controller_position = obj.Rsc.ExperimentControl.Position;
			left_corner_x = screen_size(3) - dlgWidth;
			left_corner_y = screen_size(4) - dlgHeight;

			dlg = dialog(...
				'Name', sprintf('OptoMonitor (%s)', port),...
				'WindowStyle', 'normal',...
				'OuterPosition', [left_corner_x, left_corner_y, dlgWidth, dlgHeight],...
				'Units', 'pixels',...
				'Resize', 'on',...
				'Visible', 'off'... % Hide until all controls created
			);
			% Store the dialog handle
			obj.Rsc.Monitor = dlg;

			% Size and position of controls
			dlg.UserData.DlgMargin = 20;
			dlg.UserData.LeftPanelWidthRatio = 0.3;
			dlg.UserData.PanelSpacing = 20;
			dlg.UserData.PanelMargin = 20;
			dlg.UserData.PanelMarginTop = 20;
			dlg.UserData.BarHeight = 75;
			dlg.UserData.TextHeight = 30;

			u = dlg.UserData;

			% % Close serial port when you close the window
			dlg.CloseRequestFcn = @(~, ~) (delete(gcbo));

			%----------------------------------------------------
			%		Left panel
			%----------------------------------------------------
			leftPanel = uipanel(...
				'Parent', dlg,...
				'Title', 'Experiment Summary',...
				'Units', 'pixels'...
			);
			dlg.UserData.Ctrl.LeftPanel = leftPanel;

			% Text: Number of trials completed
			trialCountText = uicontrol(...
				'Parent', leftPanel,...
				'Style', 'text',...
				'String', '# stimulations: 0',...
				'TooltipString', 'Number of stimulations completed in this session.',...
				'HorizontalAlignment', 'left',...
				'Units', 'pixels',...
				'FontSize', 13 ...
			);
			dlg.UserData.Ctrl.TrialCountText = trialCountText;

			% Text: Result of last trial
			lastTrialResultText = uicontrol(...
				'Parent', leftPanel,...
				'Style', 'text',...
				'String', 'Last stimulation: ',...
				'TooltipString', 'Last stimulation.',...
				'HorizontalAlignment', 'left',...
				'Units', 'pixels',...
				'FontSize', 13 ...
			);
			dlg.UserData.Ctrl.LastTrialResultText = lastTrialResultText;

			%----------------------------------------------------
			%		Right panel
			%----------------------------------------------------
			rightPanel = uitabgroup(...
				'Parent', dlg,...
				'Units', 'pixels'...
			);
			dlg.UserData.Ctrl.RightPanel = rightPanel;

			%----------------------------------------------------
			%		Raster Tab
			%----------------------------------------------------
			tab_raster = uitab(...
				'Parent', rightPanel,...
				'Title', 'Raster Plot'...
			);
			dlg.UserData.Ctrl.Tab_Raster = tab_raster;

			text_eventTrialStart_raster = uicontrol(...
				'Parent', tab_raster,...
				'Style', 'text',...
				'String', 'Trial Start Event',...
				'HorizontalAlignment', 'left'...				
			);
			dlg.UserData.Ctrl.Text_EventTrialStart_Raster = text_eventTrialStart_raster;

			popup_eventTrialStart_raster = uicontrol(...
				'Parent', tab_raster,...
				'Style', 'popupmenu',...
				'String', obj.Arduino.EventMarkerNames...
			);
			dlg.UserData.Ctrl.Popup_EventTrialStart_Raster = popup_eventTrialStart_raster;

			text_eventZero_raster = uicontrol(...
				'Parent', tab_raster,...
				'Style', 'text',...
				'String', 'Reference Event',...
				'HorizontalAlignment', 'left'...				
			);
			dlg.UserData.Ctrl.Text_EventZero_Raster = text_eventZero_raster;

			popup_eventZero_raster = uicontrol(...
				'Parent', tab_raster,...
				'Style', 'popupmenu',...
				'String', obj.Arduino.EventMarkerNames...
			);
			dlg.UserData.Ctrl.Popup_EventZero_Raster = popup_eventZero_raster;

			text_eventOfInterest_raster = uicontrol(...
				'Parent', tab_raster,...
				'Style', 'text',...
				'String', 'Event Of Interest',...
				'HorizontalAlignment', 'left'...
			);
			dlg.UserData.Ctrl.Text_EventOfInterest_Raster = text_eventOfInterest_raster;

			popup_eventOfInterest_raster = uicontrol(...
				'Parent', tab_raster,...
				'Style', 'popupmenu',...
				'String', obj.Arduino.EventMarkerNames...
			);
			dlg.UserData.Ctrl.Popup_EventOfInterest_Raster = popup_eventOfInterest_raster;

			text_figureName_raster = uicontrol(...
				'Parent', tab_raster,...
				'Style', 'text',...
				'String', 'Figure Name',...
				'HorizontalAlignment', 'left'...
			);
			dlg.UserData.Ctrl.Text_FigureName_Raster = text_figureName_raster;

			if isempty(obj.Arduino.SerialConnection)
				figName = 'Raster Plot';
			else
				figName = sprintf('Raster Plot (%s)', obj.Arduino.SerialConnection.Port);
			end
			edit_figureName_raster = uicontrol(...
				'Parent', tab_raster,...
				'Style', 'edit',...
				'String', figName,...
				'HorizontalAlignment', 'left'...
			);
			dlg.UserData.Ctrl.Edit_FigureName_Raster = edit_figureName_raster;

			tabgrp_plot_raster = uitabgroup(...
				'Parent', tab_raster,...
				'Units', 'pixels'...
			);
			dlg.UserData.Ctrl.TabGrp_Plot_Raster = tabgrp_plot_raster;

			tab_params_raster = uitab(...
				'Parent', tabgrp_plot_raster,...
				'Title', 'Add Parameters',...
				'Units', 'pixels'...
			);
			dlg.UserData.Ctrl.Tab_Params_Raster = tab_params_raster;			

			tab_events_raster = uitab(...
				'Parent', tabgrp_plot_raster,...
				'Title', 'Add Events',...
				'Units', 'pixels'...
			);
			dlg.UserData.Ctrl.Tab_Events_Raster = tab_events_raster;			

			paramNames = [obj.Arduino.ParamNames'; repmat({'Enter Custom Value'}, 4, 1)];
			numParams = length(paramNames);
			table_params_raster = uitable(...
				'Parent', tab_params_raster,...
				'Data', [paramNames, repmat({false}, [numParams, 1]), repmat({'black'}, [numParams, 1]), repmat({'-'}, [numParams, 1])],...
				'RowName', {},...
				'ColumnName', {'Parameter', 'Plot', 'Color', 'Style'},...
				'ColumnFormat', {'char', 'logical', {'black', 'red', 'blue', 'green', 'yellow', 'magenta', 'cyan'}, {'-', '--', ':', '-.'}},...
				'ColumnEditable', [true, true, true, true]...
			);
			dlg.UserData.Ctrl.Table_Params_Raster = table_params_raster;

			eventNames = obj.Arduino.EventMarkerNames';
			numEvents = length(eventNames);
			table_events_raster = uitable(...
				'Parent', tab_events_raster,...
				'Data', [eventNames, repmat({false}, [numEvents, 1]), repmat({'black'}, [numEvents, 1]), repmat({10}, [numEvents, 1])],...
				'RowName', {},...
				'ColumnName', {'Event', 'Plot', 'Color', 'Size'},...
				'ColumnFormat', {'char', 'logical', {'black', 'red', 'blue', 'green', 'yellow', 'magenta', 'cyan'}, 'numeric'},...
				'ColumnEditable', [false, true, true, true]...
			);
			dlg.UserData.Ctrl.Table_Events_Raster = table_events_raster;

			tab_params_raster.SizeChangedFcn = @(~, ~) OptogeneticsController.OnPlotOptionsTabResized(table_params_raster);
			tab_events_raster.SizeChangedFcn = @(~, ~) OptogeneticsController.OnPlotOptionsTabResized(table_events_raster);

			button_plot_raster = uicontrol(...
				'Parent', tab_raster,...
				'Style', 'pushbutton',...
				'String', 'Plot',...
				'Callback', @obj.Raster_GUI...
			);
			dlg.UserData.Ctrl.Button_Plot_Raster = button_plot_raster;

			%----------------------------------------------------
			%		Hist Tab
			%----------------------------------------------------
			tab_hist = uitab(...
				'Parent', rightPanel,...
				'Title', 'Histogram'...
			);
			dlg.UserData.Ctrl.Tab_Hist = tab_hist;

			text_eventTrialStart_hist = uicontrol(...
				'Parent', tab_hist,...
				'Style', 'text',...
				'String', 'Trial Start Event',...
				'HorizontalAlignment', 'left'...				
			);
			dlg.UserData.Ctrl.Text_EventTrialStart_Hist = text_eventTrialStart_hist;

			popup_eventTrialStart_hist = uicontrol(...
				'Parent', tab_hist,...
				'Style', 'popupmenu',...
				'String', obj.Arduino.EventMarkerNames...
			);
			dlg.UserData.Ctrl.Popup_EventTrialStart_Hist = popup_eventTrialStart_hist;

			text_eventZero_hist = uicontrol(...
				'Parent', tab_hist,...
				'Style', 'text',...
				'String', 'Reference Event',...
				'HorizontalAlignment', 'left'...				
			);
			dlg.UserData.Ctrl.Text_EventZero_Hist = text_eventZero_hist;

			popup_eventZero_hist = uicontrol(...
				'Parent', tab_hist,...
				'Style', 'popupmenu',...
				'String', obj.Arduino.EventMarkerNames...
			);
			dlg.UserData.Ctrl.Popup_EventZero_Hist = popup_eventZero_hist;

			text_eventOfInterest_hist = uicontrol(...
				'Parent', tab_hist,...
				'Style', 'text',...
				'String', 'Event Of Interest',...
				'HorizontalAlignment', 'left'...
			);
			dlg.UserData.Ctrl.Text_EventOfInterest_Hist = text_eventOfInterest_hist;

			popup_eventOfInterest_hist = uicontrol(...
				'Parent', tab_hist,...
				'Style', 'popupmenu',...
				'String', obj.Arduino.EventMarkerNames...
			);
			dlg.UserData.Ctrl.Popup_EventOfInterest_Hist = popup_eventOfInterest_hist;

			text_figureName_hist = uicontrol(...
				'Parent', tab_hist,...
				'Style', 'text',...
				'String', 'Figure Name',...
				'HorizontalAlignment', 'left'...
			);
			dlg.UserData.Ctrl.Text_FigureName_Hist = text_figureName_hist;
            
			if isempty(obj.Arduino.SerialConnection)
				figName = 'Histogram';
			else
				figName = sprintf('Histogram (%s)', obj.Arduino.SerialConnection.Port);
            end
            edit_figureName_hist = uicontrol(...
				'Parent', tab_hist,...
				'Style', 'edit',...
				'String', figName,...
				'HorizontalAlignment', 'left'...
			);
			dlg.UserData.Ctrl.Edit_FigureName_Hist = edit_figureName_hist;
            %----------AH 11-2-17
            text_numBins_hist = uicontrol(...
				'Parent', tab_hist,...
				'Style', 'text',...
				'String', 'Number of Bins',...
				'HorizontalAlignment', 'left'...
			);
			dlg.UserData.Ctrl.Text_NumBins_Hist = text_numBins_hist;
            numBins = 10;
			edit_numBins_hist = uicontrol(...
				'Parent', tab_hist,...
				'Style', 'edit',...
				'String', numBins,...
				'HorizontalAlignment', 'left'...
			);
			dlg.UserData.Ctrl.Edit_NumBins_Hist = edit_numBins_hist;
            %------------------------------------------------
            


			button_plot_hist = uicontrol(...
				'Parent', tab_hist,...
				'Style', 'pushbutton',...
				'String', 'Plot',...
				'Callback', @obj.Hist_GUI...
			);
			dlg.UserData.Ctrl.Button_Plot_Hist = button_plot_hist;

			%----------------------------------------------------
			% 		Stacked bar chart for trial results
			%----------------------------------------------------
			ax = axes(...
				'Parent', dlg,...
				'Units', 'pixels',...
				'XTickLabel', [],...
				'YTickLabel', [],...
				'XTick', [],...
				'YTick', [],...
				'Box', 'on'...
			);
			obj.Rsc.Monitor.UserData.Ctrl.Ax = ax;

			% Update session summary everytime a new trial's results are registered by Arduino
			obj.Arduino.Listeners.TrialRegistered = addlistener(obj.Arduino, 'TrialsCompleted', 'PostSet', @obj.OnTrialRegistered);


			%----------------------------------------------------
			% 		Menus
			%----------------------------------------------------
			menu_file = uimenu(dlg, 'Label', '&File');
			uimenu(menu_file, 'Label', '&Save Plot Settings ...', 'Callback', @(~, ~) obj.SavePlotSettings, 'Accelerator', 's');
			uimenu(menu_file, 'Label', '&Load Plot Settings ...', 'Callback', @(~, ~) obj.LoadPlotSettings, 'Accelerator', 'l');
			menu_plot = uimenu(menu_file, 'Label', '&Plot Update', 'Separator', 'on');
			menu_plot.UserData.Menu_Enable = uimenu(menu_plot, 'Label', '&Enabled', 'Callback', @(~, ~) obj.EnablePlotUpdate(menu_plot));
			menu_plot.UserData.Menu_Disable = uimenu(menu_plot, 'Label', '&Disabled', 'Callback', @(~, ~) obj.DisablePlotUpdate(menu_plot));

			if isfield(obj.UserData, 'UpdatePlot')
				if obj.UserData.UpdatePlot
					menu_plot.UserData.Menu_Enable.Checked = 'on';
					menu_plot.UserData.Menu_Disable.Checked = 'off';
				else
					menu_plot.UserData.Menu_Enable.Checked = 'off';
					menu_plot.UserData.Menu_Disable.Checked = 'on';
				end
			else
				menu_plot.UserData.Menu_Enable.Checked = 'on';
				menu_plot.UserData.Menu_Disable.Checked = 'off';
				obj.UserData.UpdatePlot = true;
			end

			menu_window = uimenu(dlg, 'Label', '&Window');
			uimenu(menu_window, 'Label', 'Optogenetics Control', 'Callback', @(~, ~) @obj.CreateDialog_ExperimentControl);
			uimenu(menu_window, 'Label', 'OptoMonitor', 'Callback', @(~, ~) obj.CreateDialog_Monitor);

			% Stretch barchart When dialog window is resized
			dlg.SizeChangedFcn = @OptogeneticsController.OnMonitorDialogResized; 

			% Unhide dialog now that all controls have been created
			dlg.Visible = 'on';
		end

		function CreateDialog_Nidaq(obj)
			% If object already exists, show window
			if isfield(obj.Rsc, 'Nidaq')
				if isvalid(obj.Rsc.Nidaq)
					figure(obj.Rsc.Nidaq)
					return
				end
			end

			% Create the dialog
			if isempty(obj.Arduino.SerialConnection)
				port = 'OFFLINE';
			else
				port = obj.Arduino.SerialConnection.Port;
			end
			dlg = dialog(...
				'Name', sprintf('NIDAQ (%s)', port),...
				'WindowStyle', 'normal',...
				'Position', [350, 450, 400, 200],...
				'Units', 'pixels',...
				'Resize', 'on',...
				'Visible', 'off'... % Hide until all controls created
			);

			% Store the dialog handle
			obj.Rsc.Nidaq = dlg;

			% Size and position of controls
			dlg.UserData.Spacing = 20;
			dlg.UserData.TextHeight = 30;

			u = dlg.UserData;

			% delete object when you close the window
			dlg.CloseRequestFcn = @(~, ~) (delete(gcbo));


			%----------------------------------------------------
			% 		Stacked bar chart for trial results
			%----------------------------------------------------
			ax = axes(...
				'Parent', dlg,...
				'Units', 'pixels',...
				'XTickLabel', [],...
				'YTickLabel', [],...
				'XTick', [],...
				'YTick', [],...
				'Box', 'on'...
			);
			obj.Rsc.Monitor.UserData.Ctrl.Ax = ax;

			% Update session summary everytime a new trial's results are registered by Arduino
			obj.Arduino.Listeners.TrialRegistered = addlistener(obj.Arduino, 'TrialsCompleted', 'PostSet', @obj.OnTrialRegistered);


			%----------------------------------------------------
			% 		Menus
			%----------------------------------------------------
			menu_file = uimenu(dlg, 'Label', '&File');
			uimenu(menu_file, 'Label', '&Save Plot Settings ...', 'Callback', @(~, ~) obj.SavePlotSettings, 'Accelerator', 's');
			uimenu(menu_file, 'Label', '&Load Plot Settings ...', 'Callback', @(~, ~) obj.LoadPlotSettings, 'Accelerator', 'l');
			menu_plot = uimenu(menu_file, 'Label', '&Plot Update', 'Separator', 'on');
			menu_plot.UserData.Menu_Enable = uimenu(menu_plot, 'Label', '&Enabled', 'Callback', @(~, ~) obj.EnablePlotUpdate(menu_plot));
			menu_plot.UserData.Menu_Disable = uimenu(menu_plot, 'Label', '&Disabled', 'Callback', @(~, ~) obj.DisablePlotUpdate(menu_plot));

			if isfield(obj.UserData, 'UpdatePlot')
				if obj.UserData.UpdatePlot
					menu_plot.UserData.Menu_Enable.Checked = 'on';
					menu_plot.UserData.Menu_Disable.Checked = 'off';
				else
					menu_plot.UserData.Menu_Enable.Checked = 'off';
					menu_plot.UserData.Menu_Disable.Checked = 'on';
				end
			else
				menu_plot.UserData.Menu_Enable.Checked = 'on';
				menu_plot.UserData.Menu_Disable.Checked = 'off';
				obj.UserData.UpdatePlot = true;
			end

			menu_window = uimenu(dlg, 'Label', '&Window');
			uimenu(menu_window, 'Label', 'Optogenetics Control', 'Callback', @(~, ~) @obj.CreateDialog_ExperimentControl);
			uimenu(menu_window, 'Label', 'OptoMonitor', 'Callback', @(~, ~) obj.CreateDialog_Monitor);

			% Stretch barchart When dialog window is resized
			% dlg.SizeChangedFcn = @OptogeneticsController.OnNiDaqDialogResized; 

			% Unhide dialog now that all controls have been created
			dlg.Visible = 'on';
		end

		function CreateDialog_Splash(obj)
			% Load image
			[fncpath, ~, ~] = fileparts(which('OptogeneticsController'));
			img = imread([fncpath, '\logo.png']);

			% Create java window object
			splashImage = im2java(img);
			win = javax.swing.JWindow;
			obj.Rsc.Splash = win;
			icon = javax.swing.ImageIcon(splashImage);
			label = javax.swing.JLabel(icon);
			win.getContentPane.add(label);
			win.setAlwaysOnTop(true);
			win.pack;

			% Set the splash image to the center of the screen
			screenSize = win.getToolkit.getScreenSize;
			screenHeight = screenSize.height;
			screenWidth = screenSize.width;
			% Get the actual splashImage size
			imgHeight = icon.getIconHeight;
			imgWidth = icon.getIconWidth;
			win.setLocation((screenWidth-imgWidth)/2,(screenHeight-imgHeight)/2);

			% Show the splash screen
			win.show

			% Hide in 10 seconds
			t = timer;
			obj.Rsc.SplashTimer = t;
			t.StartDelay = 10;
			t.TimerFcn = @(~, ~) win.dispose;
			start(t)
		end

		function CloseDialog_Splash(obj)
			stop(obj.Rsc.SplashTimer);
			obj.Rsc.Splash.dispose;
		end

		% 9-23-23 adding task scheduler functions
		%----------------------------------------------------
		% 		Task scheduler
		%----------------------------------------------------
		function CreateDialog_TaskScheduler(obj, position)
			if nargin < 2
				position = [];
				if isfield(obj.Rsc, 'ExperimentControl') && isvalid(obj.Rsc.ExperimentControl)
					position = obj.Rsc.ExperimentControl.OuterPosition(1:2) + [0, obj.Rsc.ExperimentControl.OuterPosition(4)];
				else
					position = [10 550];
				end
			end

			% If object already exists, show window
			if isfield(obj.Rsc, 'TaskScheduler')
				if isvalid(obj.Rsc.TaskScheduler)
					figure(obj.Rsc.TaskScheduler)
					return
				end
			end

			if ~isfield(obj.UserData, 'TaskSchedulerEnabled')
				obj.UserData.TaskSchedulerEnabled = false;
			end

			% Size and position of controls
			numButtons = 4;
			ctrlSpacingX = 0.05;
			ctrlSpacingY = 0.025;
			buttonWidth = (1 - (numButtons + 1)*ctrlSpacingX)/numButtons;
			buttonHeight = 0.075;
			tableWidth = 1 - 2*ctrlSpacingX;
			tableHeight = 1 - buttonHeight - 3*ctrlSpacingY;

			% Create the dialog
			if isempty(obj.Arduino.SerialConnection)
				port = 'OFFLINE';
			else
				port = obj.Arduino.SerialConnection.Port;
			end
			dlg = dialog(...
				'Name', sprintf('Task Scheduler (%s)', port),...
				'WindowStyle', 'normal',...
				'Resize', 'on',...
				'Units', 'pixels',...
				'Visible', 'off'... % Hide until all controls created
			);

			% Store the dialog handle
			obj.Rsc.TaskScheduler = dlg;

			% Create a uitable for creating tasks
			if isfield(obj.UserData, 'TaskSchedule')
				data = obj.UserData.TaskSchedule;
			else
				data = repmat({'', 'NONE', []}, [24, 1]);
			end
			table_tasks = uitable(...
				'Parent', dlg,...
				'ColumnName', {'Trials', 'Action', 'Value'},...
				'ColumnWidth', {'auto', 200, 'auto'},...
				'ColumnFormat', {'char', ['NONE', obj.Arduino.ParamNames, 'STOP'], 'long'},...
				'Data', data,...
				'ColumnEditable', true,...
				'Units', 'normalized',...
				'Position', [ctrlSpacingX, buttonHeight + 2*ctrlSpacingY, tableWidth, tableHeight] ...
			);
			dlg.UserData.Ctrl.Table_Tasks = table_tasks;

			% Apply button
			button_apply = uicontrol(...
				'Parent', dlg,...
				'Style', 'pushbutton',...
				'Units', 'normalized',...
				'Position', [ctrlSpacingX, ctrlSpacingY, buttonWidth, buttonHeight],...
				'String', 'Apply',...
				'TooltipString', 'Apply these settings.',...
				'Callback', @obj.TaskSchedulerApply ...
			);
			hPrev = button_apply;
			dlg.UserData.Ctrl.Button_Apply = hPrev;

			% Enable/disable button
			if obj.UserData.TaskSchedulerEnabled
				buttonString = 'Disable';
			else
				buttonString = 'Enable';
			end
			button_enable = uicontrol(...
				'Parent', dlg,...
				'Style', 'pushbutton',...
				'Units', 'normalized',...
				'Position', hPrev.Position,...
				'String', buttonString,...
				'TooltipString', 'Enable task scheduling.',...
				'Callback', @obj.TaskSchedulerEnable ...
			);
			hPrev = button_enable;
			dlg.UserData.Ctrl.Button_Enable = hPrev;
			hPrev.Position(1) = hPrev.Position(1) + ctrlSpacingX + buttonWidth;

			% Revert button
			button_revert = uicontrol(...
				'Parent', dlg,...
				'Style', 'pushbutton',...
				'Units', 'normalized',...
				'Position', hPrev.Position,...
				'String', 'Revert',...
				'TooltipString', 'Revert to previous setting.',...
				'Callback', @obj.TaskSchedulerRevert ...
			);
			hPrev = button_revert;
			dlg.UserData.Ctrl.Button_Revert = hPrev;
			hPrev.Position(1) = hPrev.Position(1) + ctrlSpacingX + buttonWidth;

			% Clear button
			button_clear = uicontrol(...
				'Parent', dlg,...
				'Style', 'pushbutton',...
				'Units', 'normalized',...
				'Position', hPrev.Position,...
				'String', 'Clear',...
				'TooltipString', 'Clear all settings.',...
				'Callback', @obj.TaskSchedulerClear ...
			);
			hPrev = button_clear;
			dlg.UserData.Ctrl.Button_Clear = hPrev;
			hPrev.Position(1) = hPrev.Position(1) + ctrlSpacingX + buttonWidth;

			% Menus
			menu_file = uimenu(dlg, 'Label', '&File');
			uimenu(menu_file, 'Label', '&Save Scheduler Settings ...', 'Callback', @(~, ~) obj.SaveTaskSchedulerSettings, 'Accelerator', 's');
			uimenu(menu_file, 'Label', '&Load Scheduler Settings ...', 'Callback', @(~, ~) obj.LoadTaskSchedulerSettings, 'Accelerator', 'l');
			


			menu_window = uimenu(dlg, 'Label', '&Window');
			uimenu(menu_window, 'Label', 'Experiment Control', 'Callback', @(~, ~) @obj.CreateDialog_ExperimentControl);
			uimenu(menu_window, 'Label', 'Monitor', 'Callback', @(~, ~) obj.CreateDialog_Monitor);
			uimenu(menu_window, 'Label', 'Task Scheduler', 'Callback', @(~, ~) obj.CreateDialog_TaskScheduler);
			uimenu(menu_window, 'Label', 'Camera', 'Callback', @(~, ~) obj.CreateDialog_CameraControl);


			% Resize dialog
			dlg.Position(3:4) = [430, 280];
			dlg.OuterPosition(1:2) = position;

			% Unhide dialog now that all controls have been created
			dlg.Visible = 'on';

			% fetch default params
			obj.LoadTaskSchedulerSettings('',true);

		end
		function TaskSchedulerApply(obj, ~, ~)
			obj.UserData.TaskSchedule = obj.Rsc.TaskScheduler.UserData.Ctrl.Table_Tasks.Data;
			if ~isfield(obj.UserData, 'TaskSchedulerEnabled') || ~obj.UserData.TaskSchedulerEnabled
				selection = questdlg(...
					'Enable task scheduler?',...
					'Enable',...
					'Enable','No','No'...
					);
				if strcmp(selection, 'Enable')
					obj.TaskSchedulerEnable(obj.Rsc.TaskScheduler.UserData.Ctrl.Button_Enable, [])
				end
			end
		end

		function TaskSchedulerEnable(obj, hButton, ~)
			% Default to disabled
			if ~isfield(obj.UserData, 'TaskSchedulerEnabled')
				obj.UserData.TaskSchedulerEnabled = false;
			end

			% Toggle
			obj.UserData.TaskSchedulerEnabled = ~obj.UserData.TaskSchedulerEnabled;

			% Update button
			if obj.UserData.TaskSchedulerEnabled
				hButton.String = 'Disable';
			else
				hButton.String = 'Enable';
			end
		end

		function TaskSchedulerRevert(obj, ~, ~)
			if isfield(obj.UserData, 'TaskSchedule') && ~isempty(obj.UserData.TaskSchedule)
				obj.Rsc.TaskScheduler.UserData.Ctrl.Table_Tasks.Data = obj.UserData.TaskSchedule;
			else
				obj.TaskSchedulerClear();
			end
		end

		function TaskSchedulerClear(obj, ~, ~)
			if isfield(obj.Rsc, 'TaskScheduler')
				obj.Rsc.TaskScheduler.UserData.Ctrl.Table_Tasks.Data = repmat({'', 'NONE', []}, [size(obj.Rsc.TaskScheduler.UserData.Ctrl.Table_Tasks.Data, 1), 1]);
			end
		end

		function TaskSchedulerExecute(obj)
			if ~isfield(obj.UserData, 'TaskSchedulerEnabled') || ~obj.UserData.TaskSchedulerEnabled
				return
			end

			% How many trials have been completed so far
			iTrial = obj.Arduino.TrialsCompleted;

			% Parse the list
			tasks = obj.UserData.TaskSchedule;
			isItTimeYet = cellfun(@(str) obj.TaskSchedulerIsItTimeYet(str, iTrial), tasks(:, 1));

			if ~isempty(find(isItTimeYet))
				for iTask = transpose(find(isItTimeYet))
					task = tasks{iTask, 2};
					if strcmpi(task, 'STOP')
						obj.ArduinoStop();
						break
					end
					if strcmpi(task, 'NONE')
						continue
					end
					oldValue = obj.GetParam(task);
					newValue = tasks{iTask, 3};
					if ~isempty(oldValue) && ~isempty(newValue)
						obj.SetParam(task, newValue)
					end
				end
			end
		end

		function isItTimeYet = TaskSchedulerIsItTimeYet(obj, str, iTrial)
			isItTimeYet = false;
			if isempty(str)
				return
			end

			try
				if contains(str, 'end', 'IgnoreCase', true)
					parts = strsplit(str, ':');
					if ~strcmpi(parts{end}, 'end') || nnz(cellfun(@(x) isempty(x), parts)) > 0
						error('')
					end
					switch length(parts)
						case 2
							isItTimeYet = iTrial >= str2num(parts{1});
						case 3
							isItTimeYet = iTrial >= str2num(parts{1}) && rem(iTrial - str2num(parts{1}), str2num(parts{2})) == 0;
					end
				else
					isItTimeYet = ismember(iTrial, eval(str));
				end
			catch
				warning(['Failed to evaluate task scheduler parameter (', str, ')'])
			end
		end
		%---end of 9-23-23 adding task scheduler functions


		function OnTrialRegistered(obj, ~, ~)
		% Executed when a new trial is completed
			% Autosave if a savepath is defined
			if (~isempty(obj.Arduino.ExperimentFileName) && obj.Arduino.AutosaveEnabled)
				OptogeneticsController.ArduinoSaveExperiment([], [], obj.Arduino);
			end

			% 9-23-23 task scheduler
			% Task scheduling
			obj.TaskSchedulerExecute();

			% Updated monitor window
			if isvalid(obj.Rsc.Monitor)
				% Count how many trials have been completed
				iTrial = obj.Arduino.TrialsCompleted;

				% Update the "Trials Completed" counter.
				t = obj.Rsc.Monitor.UserData.Ctrl.TrialCountText;
				t.String = sprintf('Trials completed: %d', iTrial);

				% % Show result of last trial
				% t = obj.Rsc.Monitor.UserData.Ctrl.LastTrialResultText;
				% t.String = sprintf('Last trial: %s', obj.Arduino.Trials(iTrial).CodeName);
				% Show result of last trial
				% 9-23-23
				if (iTrial > 0)
					%--- end 9-23-23
					t = obj.Rsc.Monitor.UserData.Ctrl.LastTrialResultText;
					t.String = sprintf('Last trial: %s', obj.Arduino.Trials(iTrial).CodeName);
				% 9-23-23
				end
				%--- end 9-23-23

				% When a new trial is completed
				ax = obj.Rsc.Monitor.UserData.Ctrl.Ax;

				% 9-23-23 ---
				if (iTrial > 0) % end 9-23-23
					% Get a list of currently recorded result codes
					resultCodes = reshape([obj.Arduino.Trials.Code], [], 1);
					resultCodeNames = obj.Arduino.ResultCodeNames;
					allResultCodes = 1:(length(resultCodeNames) + 1);
					resultCodeCounts = histcounts(resultCodes, allResultCodes);

					OptogeneticsController.StackedBar(ax, resultCodeCounts, resultCodeNames);
				else % 9-23-23 conditional
					cla(ax)
				end%--- end 9-23-23 conditional				
				% % Get a list of currently recorded result codes
				% resultCodes = reshape([obj.Arduino.Trials.Code], [], 1);
				% resultCodeNames = obj.Arduino.ResultCodeNames;
				% allResultCodes = 1:(length(resultCodeNames) + 1);
				% resultCodeCounts = histcounts(resultCodes, allResultCodes);

				% OptogeneticsController.StackedBar(ax, resultCodeCounts, resultCodeNames);
			end

			drawnow
		end

		%----------------------------------------------------
		%		Plot - Raster plot events for each trial
		%----------------------------------------------------
		function Raster_GUI(obj, ~, ~)
			ctrl = obj.Rsc.Monitor.UserData.Ctrl;

			eventCodeTrialStart = ctrl.Popup_EventTrialStart_Raster.Value;
			eventCodeZero 		= ctrl.Popup_EventZero_Raster.Value;
			eventCodeOfInterest = ctrl.Popup_EventOfInterest_Raster.Value;
			figName 			= ctrl.Edit_FigureName_Raster.String;
			paramPlotOptions 	= ctrl.Table_Params_Raster.Data;
			eventPlotOptions	= ctrl.Table_Events_Raster.Data;

			obj.Raster(eventCodeTrialStart, eventCodeZero, eventCodeOfInterest, figName, paramPlotOptions, eventPlotOptions)
		end
		function Raster(obj, eventCodeTrialStart, eventCodeZero, eventCodeOfInterest, figName, paramPlotOptions, eventPlotOptions)
			% First column in data is eventCode, second column is timestamp (since trial start)
			if nargin < 5
				figName = '';
			end
			if nargin < 6
				paramPlotOptions = struct([]);
			end

			% Create axes object
			f = figure('Name', figName, 'NumberTitle', 'off');

			% Store the axes object
			if ~isfield(obj.Rsc, 'LooseFigures')
				figId = 1;
			else
				figId = length(obj.Rsc.LooseFigures) + 1;
			end
			obj.Rsc.LooseFigures(figId).Ax = gca;
			ax = obj.Rsc.LooseFigures(figId).Ax;

			% Store plot settings into axes object
			ax.UserData.EventCodeTrialStart = eventCodeTrialStart;
			ax.UserData.EventCodeZero 		= eventCodeZero;
			ax.UserData.EventCodeOfInterest = eventCodeOfInterest;
			ax.UserData.FigName				= figName;
			ax.UserData.ParamPlotOptions 	= paramPlotOptions;
			ax.UserData.EventPlotOptions 	= eventPlotOptions;

			% Plot it for the first time
			obj.Raster_Execute(figId);

			% Plot again everytime an event of interest occurs
			% ax.UserData.Listener = addlistener(obj.Arduino, 'TrialsCompleted', 'PostSet', @(~, ~) obj.Raster_Execute(figId));
			if ~isfield(ax.UserData, 'Listener') || ~isvalid(ax.UserData.Listener)
				ax.UserData.Listener = addlistener(obj.Arduino, 'TrialsCompleted', 'PostSet', @(~, ~) obj.Raster_Execute(figId));
			end % conditional as of 9-23-23
			f.CloseRequestFcn = {@OptogeneticsController.OnLooseFigureClosed, ax.UserData.Listener};
		end
		function Raster_Execute(obj, figId)
			% Do not plot if the Grad Student decides we should stop plotting stuff.
			if isfield(obj.UserData, 'UpdatePlot') && (~obj.UserData.UpdatePlot)
				return
			end

			data 				= obj.Arduino.EventMarkers;
			ax 					= obj.Rsc.LooseFigures(figId).Ax;

			eventCodeTrialStart = ax.UserData.EventCodeTrialStart;
			eventCodeOfInterest = ax.UserData.EventCodeOfInterest;
			eventCodeZero 		= ax.UserData.EventCodeZero;
			figName 			= ax.UserData.FigName;
			paramPlotOptions	= ax.UserData.ParamPlotOptions;
			eventPlotOptions	= ax.UserData.EventPlotOptions;

			% If events did not occur at all, do not plot
			if (isempty(data))
				return
			end

			% Plot events
			hold(ax, 'on')
			obj.Raster_Execute_Events(ax, data, eventCodeTrialStart, eventCodeOfInterest, eventCodeZero, eventPlotOptions);

			% Plot parameters
			obj.Raster_Execute_Params(ax, paramPlotOptions);
			hold(ax, 'off')

			% Annotations
			lgd = legend(ax, 'Location', 'northoutside');
			lgd.Interpreter = 'none';
			lgd.Orientation = 'horizontal';

			ax.XLimMode 		= 'auto';
			ax.XLim 			= [max([-5000, ax.XLim(1) - 100]), ax.XLim(2) + 100];
			ax.YLim 			= [0, obj.Arduino.TrialsCompleted + 1];
			ax.YDir				= 'reverse';
			ax.XLabel.String 	= 'Time (ms)';
			ax.YLabel.String 	= 'Trial';

			% Set ytick labels
			tickInterval 	= max([1, round(ceil(obj.Arduino.TrialsCompleted/10)/5)*5]);
			ticks 			= tickInterval:tickInterval:obj.Arduino.TrialsCompleted;
			if (tickInterval > 1)
				ticks = [1, ticks];
			end
			if (obj.Arduino.TrialsCompleted) > ticks(end)
				ticks = [ticks, obj.Arduino.TrialsCompleted];
			end
			ax.YTick 		= ticks;
			ax.YTickLabel 	= ticks;
			% Set xtick labels - now is sec 9-23-23
			xtick = ax.XTick;
			ax.XTickLabel = xtick/1000;

			title(ax, figName)

			% Store plot options cause for some reason it's lost unless we do this.
			ax.UserData.EventCodeTrialStart = eventCodeTrialStart;
			ax.UserData.EventCodeZero 		= eventCodeZero;
			ax.UserData.EventCodeOfInterest = eventCodeOfInterest;
			ax.UserData.FigName 			= figName;
			ax.UserData.ParamPlotOptions 	= paramPlotOptions;
			ax.UserData.EventPlotOptions 	= eventPlotOptions;
		end
		function Raster_Execute_Events(obj, ax, data, eventCodeTrialStart, eventCodeOfInterest, eventCodeZero, eventPlotOptions)
			% Plot primary event of interest
			obj.Raster_Execute_Single(ax, data, eventCodeTrialStart, eventCodeOfInterest, eventCodeZero, 'cyan', 10)

			% Filter out events the Grad Student does not want to plot
			eventsToPlot = find([eventPlotOptions{:, 2}]);

			if ~isempty(eventsToPlot)
				for iEvent = eventsToPlot
					if iEvent == eventCodeOfInterest
						continue
					end
					obj.Raster_Execute_Single(ax, data, eventCodeTrialStart, iEvent, eventCodeZero, eventPlotOptions{iEvent, 3}, eventPlotOptions{iEvent, 4})					
				end
			end	
		end
		function Raster_Execute_Single(obj, ax, data, eventCodeTrialStart, eventCodeOfInterest, eventCodeZero, markerColor, markerSize)
			% Separate eventsOfInterest into trials, divided by eventsZero
			eventsTrialStart	= find(data(:, 1) == eventCodeTrialStart);
			eventsZero 			= find(data(:, 1) == eventCodeZero);
			eventsOfInterest 	= find(data(:, 1) == eventCodeOfInterest);

			% If events did not occur at all, do not plot
			if (isempty(eventsTrialStart) || isempty(eventsZero) || isempty(eventsOfInterest))
				return
			end

			if eventsOfInterest(end) > eventsTrialStart(end) || eventsZero(end) > eventsTrialStart(end)
				edges = [eventsTrialStart; max([eventsZero(end), eventsOfInterest(end)]) + 1];
			else
				edges = [eventsTrialStart; eventsTrialStart(end) + 1];
			end

			% Filter out 'orphan' eventsOfInterest that do not have an eventZero in the same trial 
			[~, ~, trialsOfInterest] = histcounts(eventsOfInterest, edges);
			[~, ~, trialsZero] = histcounts(eventsZero, edges);

			ism = ismember(trialsOfInterest, trialsZero);
			if (sum(ism) == 0)
				return
			end
			trialsOfInterest = trialsOfInterest(ism);
			eventsOfInterest = eventsOfInterest(ism);

			% Events of interest timestamps
			eventTimesOfInterest = data(eventsOfInterest, 2); 

			% Reference events timestamps
			if trialsOfInterest(end) > trialsZero(end)
				edges = [trialsZero; trialsOfInterest(end) + 1];
			else
				edges = [trialsZero; trialsZero(end) + 1];
			end
			[~, ~, bins] = histcounts(trialsOfInterest, edges);
			eventTimesZero = data(eventsZero(bins), 2);

			% Plot histogram of selected event times
			eventTimesRelative = eventTimesOfInterest - eventTimesZero;

			if ischar(markerColor)
				switch markerColor
					case 'black'
						markerColor = [.1, .1, .1];
					case 'red'
						markerColor = [.9, .1, .1];
					case 'blue'
						markerColor = [.1, .1, .9];
					case 'green'
						markerColor = [.1, .9, .1];
					case 'cyan'
						markerColor = [0, .5, .5];
				end
			end

			plot(ax, eventTimesRelative, trialsOfInterest, '.',...
				'MarkerSize', markerSize,...
				'MarkerEdgeColor', markerColor,...
				'MarkerFaceColor', markerColor,...
				'LineWidth', 1.5,...
				'DisplayName', obj.Arduino.EventMarkerNames{eventCodeOfInterest}...
			)
		end
		function Raster_Execute_Params(obj, ax, paramPlotOptions)
			% Plot parameters
			% Filter out parameters the Grad Student does not want to plot
			paramsToPlot = find([paramPlotOptions{:, 2}]);

			if ~isempty(paramsToPlot)
				params = ctranspose(reshape([obj.Arduino.Trials.Parameters], [], size(obj.Arduino.Trials, 2)));			
				for iParam = paramsToPlot
					% Arduino parameter
					if iParam <= length(obj.Arduino.ParamNames)
						paramValues = params(:, iParam);
					% Plot custom parameter unless the Grad Student changes its default name to a number
					elseif ~isempty(str2num(paramPlotOptions{iParam, 1}))
						paramValues = repmat(str2num(paramPlotOptions{iParam, 1}), size(params, 1), 1);
					else
						continue
					end
					plot(ax, paramValues, 1:length(obj.Arduino.Trials),...
						'DisplayName', num2str(paramPlotOptions{iParam, 1}),...
						'Color', paramPlotOptions{iParam, 3},...
						'LineStyle', paramPlotOptions{iParam, 4},...
						'LineWidth', 1.2 ...
					);
				end
			end			
		end

		%----------------------------------------------------
		%		Plot - Histogram of first licks/press duration
		%----------------------------------------------------
		function Hist_GUI(obj, ~, ~)
			ctrl = obj.Rsc.Monitor.UserData.Ctrl;

			eventCodeTrialStart = ctrl.Popup_EventTrialStart_Hist.Value;
			eventCodeZero 		= ctrl.Popup_EventZero_Hist.Value;
			eventCodeOfInterest = ctrl.Popup_EventOfInterest_Hist.Value;
			figName 			= ctrl.Edit_FigureName_Hist.String;
               %--------AH 11-2-17
            numBins             = ctrl.Edit_NumBins_Hist.String;

			obj.Hist(eventCodeTrialStart, eventCodeZero, eventCodeOfInterest, figName, numBins)
                %-----------------
        end
        %----------AH 11-2-17
		function Hist(obj, eventCodeTrialStart, eventCodeZero, eventCodeOfInterest, figName, numBins)
            %---------------
			% First column in data is eventCode, second column is timestamp (since trial start)
			if nargin < 5
				figName = '';
            end
            %------------AH 11-2-17
			if nargin < 6
				numBins = 10;
            end
            %-----------------------
			% Create axes object
			f = figure('Name', figName, 'NumberTitle', 'off');

			% Store the axes object
			if ~isfield(obj.Rsc, 'LooseFigures')
				figId = 1;
			else
				figId = length(obj.Rsc.LooseFigures) + 1;
			end
			obj.Rsc.LooseFigures(figId).Ax = gca;
			ax = obj.Rsc.LooseFigures(figId).Ax;

			% Store plot settings into axes object
			ax.UserData.EventCodeTrialStart = eventCodeTrialStart;
			ax.UserData.EventCodeZero 		= eventCodeZero;
			ax.UserData.EventCodeOfInterest = eventCodeOfInterest;
			ax.UserData.FigName				= figName;

			% Plot it for the first time
            %-------------AH 11/2
			obj.Hist_Execute(figId, numBins);
            %----------------------

			% Plot again everytime an event of interest occurs
			ax.UserData.Listener = addlistener(obj.Arduino, 'TrialsCompleted', 'PostSet', @(~, ~) obj.Hist_Execute(figId, numBins));
			f.CloseRequestFcn = {@OptogeneticsController.OnLooseFigureClosed, ax.UserData.Listener};
		end
		function Hist_Execute(obj, figId, numBins)
			% Do not plot if the Grad Student decides we should stop plotting stuff.
			if isfield(obj.UserData, 'UpdatePlot') && (~obj.UserData.UpdatePlot)
				return
			end

			data 				= obj.Arduino.EventMarkers;
			ax 					= obj.Rsc.LooseFigures(figId).Ax;

			eventCodeTrialStart = ax.UserData.EventCodeTrialStart;
			eventCodeOfInterest = ax.UserData.EventCodeOfInterest;
			eventCodeZero 		= ax.UserData.EventCodeZero;
			figName 			= ax.UserData.FigName;

			% If events did not occur at all, do not plot
			if (isempty(data))
				return
			end

			% Separate eventsOfInterest into trials, divided by eventsZero
			eventsTrialStart	= find(data(:, 1) == eventCodeTrialStart);
			eventsZero 			= find(data(:, 1) == eventCodeZero);
			eventsOfInterest 	= find(data(:, 1) == eventCodeOfInterest);

			% If events did not occur at all, do not plot
			if (isempty(eventsTrialStart) || isempty(eventsZero) || isempty(eventsOfInterest))
				return
			end

			if eventsOfInterest(end) > eventsTrialStart(end) || eventsZero(end) > eventsTrialStart(end)
				edges = [eventsTrialStart; max([eventsZero(end), eventsOfInterest(end)]) + 1];
			else
				edges = [eventsTrialStart; eventsTrialStart(end) + 1];
			end

			% Filter out 'orphan' eventsOfInterest that do not have an eventZero in the same trial 
			[~, ~, trialsOfInterest] = histcounts(eventsOfInterest, edges);
			[~, ~, trialsZero] = histcounts(eventsZero, edges);

			ism = ismember(trialsOfInterest, trialsZero);
			if (sum(ism) == 0)
				return
            end
			trialsOfInterest = trialsOfInterest(ism);
			eventsOfInterest = eventsOfInterest(ism);

			% Get timestamps for events of interests and zero events
			[C, ia, ~] 			= unique(trialsOfInterest);
			eventsZero  		= eventsZero(C);
			eventsOfInterest 	= eventsOfInterest(ia);

			eventTimesOfInterest 	= data(eventsOfInterest, 2);
			eventTimesZero 			= data(eventsZero, 2);
			
			% Substract two sets of timestamps to get relative times 
			eventTimesOfInterest 	= eventTimesOfInterest - eventTimesZero;

			% Plot histogram of selected event times
            %----------- AH 11-2-17
%             disp(numBins)
%             disp(class(numBins))
			histogram(ax, eventTimesOfInterest, str2double(numBins), 'DisplayName', obj.Arduino.EventMarkerNames{eventCodeOfInterest})
            %----------------
			lgd = legend(ax, 'Location', 'northoutside');
			lgd.Interpreter = 'none';
			lgd.Orientation = 'horizontal';
			ax.XLabel.String 	= 'Time (ms)';
			ax.YLabel.String 	= 'Occurance';
			title(ax, figName)

			% Store plot options cause for some reason it's lost unless we do this.
			ax.UserData.EventCodeTrialStart = eventCodeTrialStart;
			ax.UserData.EventCodeZero 		= eventCodeZero;
			ax.UserData.EventCodeOfInterest = eventCodeOfInterest;
			ax.UserData.FigName 			= figName;
		end

		function OnParamChanged(obj, ~, ~)
			obj.Rsc.ExperimentControl.UserData.Ctrl.Table_Params.Data = obj.Arduino.ParamValues';
		end

		function EnablePlotUpdate(obj, menu_plot)
			menu_plot.UserData.Menu_Enable.Checked = 'on';
			menu_plot.UserData.Menu_Disable.Checked = 'off';

			obj.UserData.UpdatePlot = true;
		end

		function DisablePlotUpdate(obj, menu_plot)
			menu_plot.UserData.Menu_Enable.Checked = 'off';
			menu_plot.UserData.Menu_Disable.Checked = 'on';

			obj.UserData.UpdatePlot = false;
		end

		%9-23-23 update---------------- till end of this section
		function SaveTaskSchedulerSettings(obj)
			[filename, filepath] = uiputfile('DefaultOptoTaskSchedulerParams.mat', 'Save task scheduler settings to file');
			% Exit if no file selected
			if ~(ischar(filename) && ischar(filepath))
				warning('no file selected, did not save task scheduler params')
				return
			end

			optoTaskSchedulerSettings = obj.Rsc.TaskScheduler.UserData.Ctrl.Table_Tasks.Data;

			save([filepath, filename], 'optoTaskSchedulerSettings')
		end
		
		
		function LoadTaskSchedulerSettings(obj, errorMessage,LoadFromFile)
			if nargin < 3
				LoadFromFile = false;
			end
			if LoadFromFile
				try
					stk = dbstack; 
					filepath = which(stk(1).file);
					filename = 'DefaultOptoTaskSchedulerParams.mat';
                    p = load([filepath(1:end-24), filename]);
				catch
					warning('Could not load default task scheduler params. Save default params as DefaultOptoTaskSchedulerParams.mat to folder containing OptogeneticsController')
				end
			else
				if nargin < 2
					errorMessage = '';
				end
				% Display errorMessage prompt if called for
				if ~isempty(errorMessage)
					selection = questdlg(...
						errorMessage,...
						'Error',...
						'Yes','No','Yes'...
					);
					% Exit if the Grad Student says 'No'
					if strcmp(selection, 'No')
						return
					end
				end

				[filename, filepath] = uigetfile('*.mat', 'Load task scheduler settings from file');
				% Exit if no file selected
				if ~(ischar(filename) && ischar(filepath))
					return
				end
				% Load file
                p = load([filepath, filename]);
			end
			
			% If loaded file does not contain parameters
			if ~isfield(p, 'optoTaskSchedulerSettings')
				% Ask the Grad Student if he wants to select another file instead
				obj.LoadPlotSettings('The file you selected was not loaded because it does not contain task scheduler settings. Select another file instead?')
			else
				% If all checks are good then do the deed
				% obj.UserData.TaskSchedule = obj.Rsc.TaskScheduler.UserData.Ctrl.Table_Tasks.Data
				ctrl = obj.Rsc.TaskScheduler.UserData.Ctrl;
				ctrl.Table_Tasks.Data = p.optoTaskSchedulerSettings;
				obj.UserData.TaskSchedule = obj.Rsc.TaskScheduler.UserData.Ctrl.Table_Tasks.Data;
                %
			end
		end


		function varargout = GetParam(obj, index)
			p = inputParser;
			addRequired(p, 'Index', @(x) isnumeric(x) || ischar(x));
			parse(p, index);
			index = p.Results.Index;

			if ischar(index)
				index = find(strcmpi(index, obj.Arduino.ParamNames));
			end

			if isempty(index)
				varargout = {[]};
			else
				index = index(1);
				varargout = {obj.Arduino.ParamValues(index)};
			end
		end

		function SetParam(obj, index, value, varargin)
			p = inputParser;
			addRequired(p, 'Index', @(x) isnumeric(x) || ischar(x));
			addRequired(p, 'Value', @isnumeric);
			parse(p, index, value);
			index = p.Results.Index;
			value = p.Results.Value;

			if ischar(index)
				index = find(strcmpi(index, obj.Arduino.ParamNames));
			end

			if ~isempty(index)
				index = index(1);
				obj.Arduino.UpdateParams_AddToQueue(index, value);
				obj.Arduino.UpdateParams_Execute();
			end
		end %---end 9-23-23 update
	end

	%----------------------------------------------------
	%		Static methods
	%----------------------------------------------------
	methods (Static)
		%----------------------------------------------------
		%		Commmunicating with Arduino
		%----------------------------------------------------
		function arduinoPortName = QueryPort()
			
			serialInfo = instrhwinfo('serial');

			if isempty(serialInfo.AvailableSerialPorts)
				serialPorts = {'Nothing connected'};
			else
				serialPorts = serialInfo.AvailableSerialPorts;
			end

			[selection, online] = listdlg(...
				'ListString', serialPorts,...
				'SelectionMode', 'single',...
				'ListSize', [100, 75],...
				'PromptString', 'Select COM port',...
				'CancelString', 'Offline'...
			);

			if isempty(serialInfo.AvailableSerialPorts)
				arduinoPortName = '/offline';
				return
			end

			if online
				arduinoPortName = serialInfo.AvailableSerialPorts{selection};
			else
				arduinoPortName = '/offline';
			end	
		end

		function OnParamChangedViaGUI(~, evnt, arduino)
			% evnt (event data contains infomation on which elements were changed to what)
			changedParam = evnt.Indices(1);
			newValue = evnt.NewData;
			
			% Add new parameter to update queue
			arduino.UpdateParams_AddToQueue(changedParam, newValue)
			% Attempt to execute update queue now, if current state does not allow param update, the queue will be executed when we enter an appropriate state
			arduino.UpdateParams_Execute()
		end
		function ArduinoStart(~, ~, arduino)
			if ((~arduino.AutosaveEnabled) || isempty(arduino.ExperimentFileName))
				selection = questdlg(...
					'Autosave is disabled. Start experiment anyway?',...
					'Autosave',...
					'Save', 'Start Anyway', 'Cancel' ,'Save'...
				);
				switch selection
					case 'Save'
						arduino.SaveAsExperiment()
					case 'Start Anyway'
						warning('Autosave not enabled. Starting experiment anyway.')
					otherwise
						return
				end 
			end
			arduino.Start()
			fprintf('Started.\n')			
		end
		function ArduinoStop(~, ~, arduino)
			arduino.Stop()
			fprintf('Stopped.\n')
		end
		function ArduinoReset(~, ~, arduino)
			arduino.Reset()
			fprintf('Reset.\n')
		end
		function StimulateChR2(~, ~, arduino)
			arduino.ChR2()
			fprintf('Stimulating ChR2 with UI parameters...\n')
		end
		function Optotag(~, ~, arduino)
			arduino.Optotag()
			fprintf('Called Optotagging ladder...\n')
		end
		function ArduinoClose(~, ~, obj)
			selection = questdlg(...
				'Close all windows and terminate connection with Arduino?',...
				'Close Window',...
				'Yes','No','Yes'...
			);
			switch selection
				case 'Yes'
					obj.Arduino.Close()
					delete(obj.Rsc.Monitor)
					delete(obj.Rsc.ExperimentControl)
					delete(obj.Rsc.TaskScheduler)
					fprintf('Connection closed.\n')
				case 'No'
					return
			end
		end
		function ArduinoReconnect(~, ~, arduino)
			arduino.Reconnect()
		end
		function ArduinoSaveParameters(~, ~, arduino)
			arduino.SaveParameters()
		end
		function ArduinoLoadParameters(~, ~, arduino, table_params)
			if nargin < 4
				table_params = [];
			end
			arduino.LoadParameters(table_params, '')
		end
		function ArduinoSaveExperiment(~, ~, arduino)
			if isempty(arduino.ExperimentFileName)
				arduino.SaveAsExperiment()
			else
				arduino.SaveExperiment()
			end
		end
		function ArduinoSaveAsExperiment(~, ~, arduino)
			arduino.SaveAsExperiment()
		end
		function ArduinoLoadExperiment(~, ~, arduino)
			arduino.LoadExperiment()
		end

		%----------------------------------------------------
		%		Commmunicating with DAQ
		%----------------------------------------------------
		function NidaqStart(~, ~, nidaq)
			nidaq.Start()
		end

		function NidaqStop(~, ~, nidaq)
			nidaq.Stop()
		end


		%----------------------------------------------------
		%		Dialog Resize callbacks
		%----------------------------------------------------
		function OnMonitorDialogResized(~, ~)
			% Retrieve dialog object and axes to resize
			dlg = gcbo;
			ax = dlg.UserData.Ctrl.Ax;
			leftPanel = dlg.UserData.Ctrl.LeftPanel;
			rightPanel = dlg.UserData.Ctrl.RightPanel;
			trialCountText = dlg.UserData.Ctrl.TrialCountText;
			lastTrialResultText = dlg.UserData.Ctrl.LastTrialResultText;


			text_eventTrialStart_raster = dlg.UserData.Ctrl.Text_EventTrialStart_Raster;
			popup_eventTrialStart_raster = dlg.UserData.Ctrl.Popup_EventTrialStart_Raster;
			text_eventZero_raster = dlg.UserData.Ctrl.Text_EventZero_Raster;
			popup_eventZero_raster = dlg.UserData.Ctrl.Popup_EventZero_Raster;
			text_eventOfInterest_raster = dlg.UserData.Ctrl.Text_EventOfInterest_Raster;
			popup_eventOfInterest_raster = dlg.UserData.Ctrl.Popup_EventOfInterest_Raster;
			text_figureName_raster = dlg.UserData.Ctrl.Text_FigureName_Raster;
			edit_figureName_raster = dlg.UserData.Ctrl.Edit_FigureName_Raster;
			tabgrp_plot_raster = dlg.UserData.Ctrl.TabGrp_Plot_Raster;
			button_plot_raster = dlg.UserData.Ctrl.Button_Plot_Raster;

			text_eventTrialStart_hist = dlg.UserData.Ctrl.Text_EventTrialStart_Hist;
			popup_eventTrialStart_hist = dlg.UserData.Ctrl.Popup_EventTrialStart_Hist;
			text_eventZero_hist = dlg.UserData.Ctrl.Text_EventZero_Hist;
			popup_eventZero_hist = dlg.UserData.Ctrl.Popup_EventZero_Hist;
			text_eventOfInterest_hist = dlg.UserData.Ctrl.Text_EventOfInterest_Hist;
			popup_eventOfInterest_hist = dlg.UserData.Ctrl.Popup_EventOfInterest_Hist;
			text_figureName_hist = dlg.UserData.Ctrl.Text_FigureName_Hist;
            %-----AH 11-2-17
            text_numBins_hist = dlg.UserData.Ctrl.Text_NumBins_Hist;
            edit_numBins_hist = dlg.UserData.Ctrl.Edit_NumBins_Hist;
            %-----------------------------------
			edit_figureName_hist = dlg.UserData.Ctrl.Edit_FigureName_Hist;
			button_plot_hist = dlg.UserData.Ctrl.Button_Plot_Hist;

			u = dlg.UserData;
			
			% Bar plot axes should have constant height.
			ax.Position = [...
				u.DlgMargin,...
				u.DlgMargin,...
				dlg.Position(3) - 2*u.DlgMargin,...
				u.BarHeight...
			];

			% Left and right panels extends in width and height to fill dialog.
			leftPanel.Position = [...
				u.DlgMargin,...
				u.DlgMargin + u.BarHeight + u.PanelSpacing,...
				max([0, u.LeftPanelWidthRatio*(dlg.Position(3) - 2*u.DlgMargin - u.PanelSpacing)]),...
				max([0, dlg.Position(4) - 2*u.DlgMargin - u.PanelSpacing - u.BarHeight])...
			];				
			rightPanel.Position = [...
				u.DlgMargin + leftPanel.Position(3) + u.PanelSpacing,...
				u.DlgMargin + u.BarHeight + u.PanelSpacing,...
				max([1, (1 - u.LeftPanelWidthRatio)*(dlg.Position(3) - 2*u.DlgMargin - u.PanelSpacing)]),...
				leftPanel.Position(4)...
			];

			% Trial count text should stay on top left of leftPanel
			trialCountText.Position = [...
				u.PanelMargin,...
				leftPanel.Position(4) - u.PanelMargin - 1.2*u.TextHeight,...
				max([0, leftPanel.Position(3) - 2*u.PanelMargin]),...
				u.TextHeight...
			];

			lastTrialResultText.Position = trialCountText.Position;
			lastTrialResultText.Position(2) = trialCountText.Position(2) - 2.4*u.TextHeight;
			lastTrialResultText.Position(4) = 2.4*trialCountText.Position(4);

			% Raster plot tab
			plotOptionWidth = (rightPanel.Position(3) - 4*u.PanelMargin)/3;
			text_eventTrialStart_raster.Position = [...
				u.PanelMargin,...
				rightPanel.Position(4) - u.PanelMargin - u.PanelMarginTop - text_eventTrialStart_raster.Extent(4),...
				plotOptionWidth,...
				text_eventTrialStart_raster.Extent(4)...
			];

			popup_eventTrialStart_raster.Position = text_eventTrialStart_raster.Position;
			popup_eventTrialStart_raster.Position(2) =...
				text_eventTrialStart_raster.Position(2) - text_eventTrialStart_raster.Position(4);

			text_eventZero_raster.Position = text_eventTrialStart_raster.Position;
			text_eventZero_raster.Position(1) =...
				text_eventTrialStart_raster.Position(1) + text_eventTrialStart_raster.Position(3) + u.PanelMargin;

			popup_eventZero_raster.Position = text_eventZero_raster.Position;
			popup_eventZero_raster.Position(2) =...
				text_eventZero_raster.Position(2) - text_eventZero_raster.Position(4);

			text_eventOfInterest_raster.Position = text_eventZero_raster.Position;
			text_eventOfInterest_raster.Position(1) =...
				text_eventZero_raster.Position(1) + text_eventZero_raster.Position(3) + u.PanelMargin;

			popup_eventOfInterest_raster.Position = text_eventOfInterest_raster.Position;
			popup_eventOfInterest_raster.Position(2) =...
				text_eventOfInterest_raster.Position(2) - text_eventOfInterest_raster.Position(4);

			tabgrp_plot_raster.Position = popup_eventTrialStart_raster.Position;
			tabgrp_plot_raster.Position(2:4) = [...
				u.PanelMargin,...
				max([1, 2*plotOptionWidth + u.PanelMargin]),...
				max([1, popup_eventTrialStart_raster.Position(2) - 2*u.PanelMargin]),...
			];

			text_figureName_raster.Position = popup_eventOfInterest_raster.Position;
			text_figureName_raster.Position(2) = popup_eventOfInterest_raster.Position(2) - u.PanelMargin - text_figureName_raster.Position(4);

			edit_figureName_raster.Position = text_figureName_raster.Position;
			edit_figureName_raster.Position(2) =...
				text_figureName_raster.Position(2) - text_figureName_raster.Position(4);

			button_plot_raster.Position = edit_figureName_raster.Position;
			button_plot_raster.Position([2, 4]) = [...
				tabgrp_plot_raster.Position(2),...
				2*text_eventZero_raster.Position(4)...
			];

			% Histogram tab
			text_eventTrialStart_hist.Position = text_eventTrialStart_raster.Position;
			popup_eventTrialStart_hist.Position = popup_eventTrialStart_raster.Position;
			text_eventZero_hist.Position = text_eventZero_raster.Position;
			popup_eventZero_hist.Position = popup_eventZero_raster.Position;
			text_eventOfInterest_hist.Position = text_eventOfInterest_raster.Position;
			popup_eventOfInterest_hist.Position = popup_eventOfInterest_raster.Position;
			text_figureName_hist.Position = text_figureName_raster.Position;
			edit_figureName_hist.Position = edit_figureName_raster.Position;
            %---------AH 11-2-17
            text_numBins_hist.Position = text_figureName_raster.Position - [200,0,0,0];
			edit_numBins_hist.Position = edit_figureName_raster.Position - [200, 0,0,0];
            %---------------------------
			button_plot_hist.Position = button_plot_raster.Position;

		end

		function OnPlotOptionsTabResized(table)
			tab = gcbo;
			table.Position = tab.Position;
			table.ColumnWidth = num2cell((table.Position(3) - 17)*[0.5, 0.5/3, 0.5/3, 0.5/3]);
		end

		%----------------------------------------------------
		%		Loose figure closed callback
		%----------------------------------------------------
		% Stop updating figure when we close it
		function OnLooseFigureClosed(src, ~, lh)
			delete(lh)
			delete(src)
		end

		%----------------------------------------------------
		%		Plot - Stacked Bar
		%----------------------------------------------------
		function bars = StackedBar(ax, data, names, colors)
			% Default params
			if nargin < 4
				colors = {[.2, .8, .2], [1 .2 .2], [.9 .2 .2], [.8 .2 .2], [.7 .2 .2]};
			end

			% Create a stacked horizontal bar plot
			data = reshape(data, 1, []);
			bars = barh(ax, [data; nan(size(data))], 'stack');
			
			% Remove whitespace and axis ticks
			ax.XLim = [0, sum(data)];
			ax.YLim = [1 - 0.5*bars(1).BarWidth, 1 + 0.5*bars(1).BarWidth];
			ax.XTickLabel = [];
			ax.YTickLabel = [];
			ax.XTick = [];
			ax.YTick = [];

			% Add labels and set color
			edges = [0, cumsum(data)];
			
			for iData = 1:length(data)
				percentage = round(data(iData)/sum(data) * 100);
				labelLong = sprintf('%s \n%dx\n(%d%%)', names{iData}, data(iData), percentage);
				labelMed = sprintf('''%d''\n%dx', iData, data(iData));
				labelShort = sprintf('''%d''', iData);
				center = (edges(iData) + edges(iData + 1))/2;

				t = text(ax, center, 1, labelLong,...
					'HorizontalAlignment', 'center',...
					'VerticalAlignment', 'middle',...
					'Interpreter', 'none'...
				);
				bars(iData).UserData.tLong = t;

				% Hide parts of the label if it's wider than the bar
				if (t.Extent(3) > data(iData))
					t.Visible = 'off';
					t = text(ax, center, 1, labelMed,...
						'HorizontalAlignment', 'center',...
						'VerticalAlignment', 'middle',...
						'Interpreter', 'none'...
					);
					bars(iData).UserData.tMed = t;
					if (t.Extent(3) > data(iData))
						t.Visible = 'off';
						t = text(ax, center, 1, labelShort,...
							'HorizontalAlignment', 'center',...
							'VerticalAlignment', 'middle',...
							'Interpreter', 'none'...
						);
						bars(iData).UserData.tShort = t;
						if (t.Extent(3) > data(iData))
							t.Visible = 'off';
						end						
					end
				end

				% Show bar text when clicked
				bars(iData).ButtonDownFcn = @OptogeneticsController.OnStackedBarSingleClick;

				% Set bar color
				if iData <= length(colors)
					thisColor = colors{iData};
					bars(iData).FaceColor = thisColor;
				else
					% If color not defined, fade "R" from .7 to .2
					iOverflow = iData - length(colors);
					totalOverflow = length(data) - length(colors);

					decrement = 0.5/totalOverflow;
					r = 0.7 - decrement*iOverflow;
					bars(iData).FaceColor = [r .2 .2];
				end
			end
		end

		function OnStackedBarSingleClick(h, ~)
			% If text from another bar is force shown, hide it first
			t = findobj(gca, 'Tag', 'ForceShown');
			if ~isempty(t)
				t.Tag = '';
				t.Visible = 'off';
				t.BackgroundColor = 'none';
				t.EdgeColor = 'none';
			end

			% Force show text associated with bar clicked, unless it's already shown, in which case clicking again will hide it
			tLong = h.UserData.tLong;
			if strcmp(tLong.Visible, 'off')
				if ~isempty(t)
					if t == tLong
						return
					end
				end
				tLong.Tag = 'ForceShown';
				tLong.Visible = 'on';
				tLong.BackgroundColor = [1 1 .73];
				tLong.EdgeColor = 'k';
				uistack(tLong, 'top') % Bring to top
			end
		end
	end
end