% printFigure.m
function printFigure(name, f)
	% 
	% 	Created 2/2020	Allison Hamilos 	ahamilos{at}g.harvard.edu
	% 
	if nargin < 3
		f = gcf;
	end
	print(f,'-depsc','-painters', [name, '.eps'])
	savefig(f, name);
end