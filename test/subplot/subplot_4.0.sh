#!/usr/bin/env bash
#
# Testing all combinations of subplot behaviors with or without -S and how MAP_FRAME_AXES and -Bframes affect things
# subplot_1.x uses default settings and no -Sr -Sc
# subplot_2.x uses -Sc
# subplot_3.x uses -Sr
# subplot_4.x uses -Sc -Sr
# subplot_x.0 uses default settings
# subplot_x.1 sets MAP_FRAME_AXES via subplot begin
# subplot_x.2 adds -Bframes via subplot begin
# subplot_x.3 adds -Bframes just for one panel (LL)
# This results in 16 different 2x2 subplot examples

# 4.0 -SR -SC no override
gmt begin subplot_4.0
	gmt set FONT_HEADING 28p,Helvetica,black
    gmt subplot begin 2x2 -Fs8c -Scb -Srl -R-2/2/0/10 -T"4.0: -Srl -Scb"
       	gmt basemap -c
        gmt basemap -c
    	gmt basemap -c
      	gmt basemap -c
    gmt subplot end
gmt end show
