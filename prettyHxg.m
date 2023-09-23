function h = prettyHxg(ax, data, displayname, color, edges, nbins, Normalization)
    if nargin < 7
        Normalization = 'probability';
    end
    if nargin < 3
        displayname = 'data';
    end
    if nargin < 4
        color = 'k';
    end
    if (isempty(edges) && isempty(nbins))
	    h = histogram(ax, data, 'displaystyle', 'stairs', 'normalization', Normalization, 'linewidth', 3, 'edgecolor', color, 'displayname', displayname);
    elseif ~isempty(edges)
		h = histogram(ax, data, 'displaystyle', 'stairs', 'normalization', Normalization, 'linewidth', 3, 'edgecolor', color, 'displayname', displayname, 'binedges', edges);
	elseif isempty(edges)
		h = histogram(ax, data, 'displaystyle', 'stairs', 'normalization', Normalization, 'linewidth', 3, 'edgecolor', color, 'displayname', displayname, 'numbins', nbins);
	end
end