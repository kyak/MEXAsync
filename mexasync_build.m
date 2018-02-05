% Assume that cares.dll and cares.lib are in the same folder as mex
% Get c-ares library here: https://github.com/c-ares/c-ares 
mex mexasync.c -Icares -lcares -lWs2_32
