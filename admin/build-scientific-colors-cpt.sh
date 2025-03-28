#!/usr/bin/env bash
#
# This script takes the downloaded zip content from
# Crameri, Fabio. (2023, Oct 5). Scientific colour maps
# (Version 8.0.1). Zenodo.  https://doi.org/10.5281/zenodo.1243862
# and converts the *.cpt files into proper GMT master
# CPT files with correct attribution and hinge info
# Run from the ScientificColourMapsV directory (V is version) after the
# zip has downloaded and been expanded.  But first you need to manually
# update the $TMP/cpt.info entries below with one line per CPT subdir in the
# downloaded directory.  It will create a gmt subdirectory with all the CPTs.
# You also need to edit gmt_cpt_masters.h after adding the CPTs to share/cpt
#
# Last setup and run for ScientificColourMaps8 (release 8.0.1) on Jan 2, 2025 for GMT 6.6 (master)
# Gave 56 CPTs: The same as release 8.0.0 from GMT 6.5 with new entry naviaW.
#

if [ $# -eq 0 ]; then
	cat <<- EOF  >&2
	Usage: build-scientific-colors-cpt.sh <SCM-directory>

	Will create the GMT CPT versions of Crameri's scientific colour maps.
	Give the full path to the expanded zip file top directory, such as
	~/Download/ScientificColourMaps8.
	Before running you must update this script with:
	  1. Any new CPT entries since the last release to \$TMP/cpt.info
	  2. Flag those with a soft hinge as S and a hard hinge as H
	  3. Manually set the current version number/doi (see the zip PDF docs)
	Afterwards you must:
	  1. Update src/gmt_cpt_masters.h with any new entries (copy lines from \$DIR/cpt_strings.txt)
	  2. Adding the CPTs to share/cpt/SCM (overwriting the previous versions)
	  3. Probably mess with doc/scripts/GMT_App_M*.sh for new layouts
	EOF
	exit 1
fi

DIR=$1
VERSION=8.0.1
DOI=10.5281/zenodo.1243862

TMP=$(mktemp -d) 
cat << EOF > $TMP/cpt.info
acton|Perceptually uniform sequential colormap, by Fabio Crameri [C=RGB]
actonS|Perceptually uniform sequential categorical colormap, by Fabio Crameri [C=RGB]
bam|Perceptually uniform bimodal colormap, light, by Fabio Crameri [C=RGB]
bamO|Perceptually uniform bimodal cyclic colormap, light, by Fabio Crameri [C=RGB]
bamako|Perceptually uniform, low-lightness gradient colormap by Fabio Crameri [C=RGB]
bamakoS|Perceptually uniform, low-lightness gradient categorical colormap by Fabio Crameri [C=RGB]
batlow|Perceptually uniform 'rainbow' colormap by Fabio Crameri [C=RGB]
batlowK|Perceptually uniform 'rainbow' colormap with black ending by Fabio Crameri [C=RGB]
batlowS|Perceptually uniform 'rainbow' categorical colormap by Fabio Crameri [C=RGB]
batlowW|Perceptually uniform 'rainbow' colormap with white ending by Fabio Crameri [C=RGB]
berlin|Perceptually uniform bimodal colormap, dark, by Fabio Crameri [S,C=RGB]
bilbao|Perceptually uniform colormap by Fabio Crameri [C=RGB]
bilbaoS|Perceptually uniform categorical colormap by Fabio Crameri [C=RGB]
broc|Perceptually uniform bimodal colormap, light, by Fabio Crameri [S,C=RGB]
brocO|Perceptually uniform bimodal cyclic colormap, light, by Fabio Crameri [C=RGB]
buda|Perceptually uniform, low-lightness gradient colormap, by Fabio Crameri [C=RGB]
budaS|Perceptually uniform, low-lightness gradient categorical colormap, by Fabio Crameri [C=RGB]
bukavu|Perceptually uniform multi-sequential colormap by Fabio Crameri [H,C=RGB]
cork|Perceptually uniform bimodal colormap, light, by Fabio Crameri [S,C=RGB]
corkO|Perceptually uniform bimodal cyclic colormap, light, by Fabio Crameri [C=RGB]
davos|Perceptually uniform colormap by Fabio Crameri [C=RGB]
davosS|Perceptually uniform categorical colormap by Fabio Crameri [C=RGB]
devon|Perceptually uniform sequential colormap, by Fabio Crameri [C=RGB]
devonS|Perceptually uniform sequential categorical colormap, by Fabio Crameri [C=RGB]
fes|Perceptually uniform multi-sequential colormap by Fabio Crameri [H,C=RGB]
glasgow|Perceptually uniform sequential colormap by Fabio Crameri [C=RGB]
grayC|Perceptually uniform 'gray' colormap by Fabio Crameri [C=RGB]
grayCS|Perceptually uniform 'gray' categorical colormap by Fabio Crameri [C=RGB]
hawaii|Perceptually uniform lush colormap by Fabio Crameri [C=RGB]
hawaiiS|Perceptually uniform lush categorical colormap by Fabio Crameri [C=RGB]
imola|Perceptually uniform, low-lightness gradient colormap, by Fabio Crameri [C=RGB]
imolaS|Perceptually uniform, low-lightness gradient categorical colormap, by Fabio Crameri [C=RGB]
lajolla|Perceptually uniform colormap, without black or white, by Fabio Crameri [C=RGB]
lajollaS|Perceptually uniform categorical colormap, without black or white, by Fabio Crameri [C=RGB]
lapaz|Perceptually uniform 'rainbow' colormap by Fabio Crameri [C=RGB]
lapazS|Perceptually uniform 'rainbow' categorical colormap by Fabio Crameri [C=RGB]
lisbon|Perceptually uniform bimodal colormap, dark, by Fabio Crameri [S,C=RGB]
lipari|Perceptually uniform sequential colormap by Fabio Crameri [C=RGB]
managua|Perceptually uniform diverging colormap, by Fabio Crameri [S,C=RGB]
navia|Perceptually uniform sequential colormap by Fabio Crameri [C=RGB]
naviaW|Perceptually uniform sequential colormap with white ending by Fabio Crameri [C=RGB]
nuuk|Perceptually uniform, low-lightness gradient colormap, by Fabio Crameri [C=RGB]
nuukS|Perceptually uniform, low-lightness gradient categorical colormap, by Fabio Crameri [C=RGB]
oleron|Perceptually uniform topography colormap, by Fabio Crameri [H,C=RGB]
oslo|Perceptually uniform, B&W limits, by Fabio Crameri [C=RGB]
osloS|Perceptually uniform, B&W limits, categorical colormap, by Fabio Crameri [C=RGB]
roma|Perceptually uniform 'seis' colormap by Fabio Crameri [S,C=RGB]
romaO|Perceptually uniform cyclic colormap by Fabio Crameri [C=RGB]
tofino|Perceptually uniform bimodal colormap, dark, by Fabio Crameri [S,C=RGB]
tokyo|Perceptually uniform colormap without black or white, by Fabio Crameri [C=RGB]
tokyoS|Perceptually uniform categorical colormap without black or white, by Fabio Crameri [C=RGB]
turku|Perceptually uniform colormap by Fabio Crameri [C=RGB]
turkuS|Perceptually uniform categorical colormap by Fabio Crameri [C=RGB]
vanimo|Perceptually uniform bimodal colormap, dark, by Fabio Crameri [C=RGB]
vik|Perceptually uniform bimodal colormap, light, by Fabio Crameri [S,C=RGB]
vikO|Perceptually uniform bimodal cyclic colormap, light, by Fabio Crameri [C=RGB]
EOF
cd $DIR
# Make formatted list of lines suitable for copying into gmt_cpt_masters.h
awk -F'|' '{printf "\"SCM/%-14s : %s\",\n", $1, $2}' $TMP/cpt.info > $TMP/cpt_strings.txt
# Make list of CPTs with a hinge of some soft since these need to insert a true z = 0 slice
grep "\[H," $TMP/cpt.info | awk -F'|' '{print $1}' > $TMP/hinge.lis
grep "\[S," $TMP/cpt.info | awk -F'|' '{print $1}' >> $TMP/hinge.lis

rm -rf gmt_cpts
mkdir gmt_cpts
cat <<- EOF > $TMP/front
#
#----------------------------------------------------------
# COLOR_MODEL = RGB
EOF
while read line; do
	cpt=$(echo $line | awk -F'|' '{print $1}')
	cat <<- EOF > gmt_cpts/$cpt.cpt
	#
	EOF
	echo $line | awk -F'|' '{printf "# SCM/%s : %s\n", $1, $2}' >> gmt_cpts/$cpt.cpt
	last_char=$(echo $cpt | awk '{print substr($1,length($1),1)}')
	if [ "X${last_char}" = "XS" ]; then
		tmp=$(echo $cpt | awk '{print substr($1,1, length($1)-1)}')
		cptdir=${tmp}/CategoricalPalettes
	else
		cptdir=${cpt}
	fi
	cat <<- EOF >> gmt_cpts/$cpt.cpt
	#
	#	www.fabiocrameri.ch/visualisation
	#
	# License: MIT License
	# Copyright (c) 2023, Fabio Crameri.
	# Crameri, F., (2023). Scientific colour maps. Zenodo. https://doi.org/$DOI
	# This is Scientific Colour Maps version $VERSION
	# Note: Original file converted to GMT version >= 5 CPT format.
	EOF
	#if [ "$cpt" = "broc" ] || [ "$cpt" = "cork" ] || [ "$cpt" = "vik" ] || [ "$cpt" = "lisbon" ] || [ "$cpt" = "tofino" ] || [ "$cpt" = "berlin" ] || [ "$cpt" = "oleron" ] ; then
	case $line in
	*\[H*) hinge="HARD_HINGE" ;;
	*\[S*) hinge="SOFT_HINGE" ;;
	    *) hinge="" ;;
	esac
	if [ "X${last_char}" = "XS" ]; then
		cat $TMP/front >> gmt_cpts/$cpt.cpt
		echo "#----------------------------------------------------------" >> gmt_cpts/$cpt.cpt
		egrep -v '^#|^F|^B|^N' $cptdir/$cpt.cpt | awk '{if (NR == 1) { printf "%d\t%s/%s/%s\n%d\t%s/%s/%s\n", 0, $2, $3, $4, 1, $6, $7, $8} else {printf "%d\t%s/%s/%s\n", NR, $6, $7, $8}}' > $TMP/tmp.cpt
	elif [ "X$hinge" = "X" ]; then
		cat $TMP/front >> gmt_cpts/$cpt.cpt
		if [ "X${last_char}" = "XO" ]; then
			echo "# CYCLIC" >> gmt_cpts/$cpt.cpt
		fi
		echo "#----------------------------------------------------------" >> gmt_cpts/$cpt.cpt
		egrep -v '^#|^F|^B|^N' $cptdir/$cpt.cpt | awk '{printf "%.6f\t%s/%s/%s\t%.6f\t%s/%s/%s\n", $1, $2, $3, $4, $5, $6, $7, $8}' > $TMP/tmp.cpt
	else
		echo "# Note: Range changed from 0-1 to -1/+1 to place hinge at zero." >> gmt_cpts/$cpt.cpt
		cat $TMP/front >> gmt_cpts/$cpt.cpt
		echo "# $hinge" >> gmt_cpts/$cpt.cpt
		echo "#----------------------------------------------------------" >> gmt_cpts/$cpt.cpt
		# Convert to -1/1 range
		egrep -v '^#|^F|^B|^N' $cptdir/$cpt.cpt | awk '{printf "%.6f\t%s/%s/%s\t%.6f\t%s/%s/%s\n", 2*($1-0.5), $2, $3, $4, 2*($5-0.5), $6, $7, $8}' > $TMP/tmp.cpt
	fi
	cat $TMP/tmp.cpt >> gmt_cpts/$cpt.cpt
	if [ "X${last_char}" = "XS" ] || [ "X${last_char}" = "XO" ]; then	# Categorical or cyclical CPTS have no F or B, only NaN
		egrep '^N' $cptdir/$cpt.cpt | awk '{printf "%s\t%s/%s/%s\n", $1, $2, $3, $4}' >> gmt_cpts/$cpt.cpt
	else
		egrep '^F|^B|^N' $cptdir/$cpt.cpt | awk '{printf "%s\t%s/%s/%s\n", $1, $2, $3, $4}' >> gmt_cpts/$cpt.cpt
	fi
done < $TMP/cpt.info
# Fix the zero hinges
while read cpt; do
	grep '^#' gmt_cpts/${cpt}.cpt > $TMP/${cpt}.cpt
	egrep -v '^#|B|N|F' gmt_cpts/${cpt}.cpt | awk '{if (NR == 127) {printf "%s\t%s\t0.0\t\t\t%s\n", $1, $2, $4} else if (NR == 129) {printf "0.0\t\t\t%s\t%s\t%s\n", $2, $3, $4} else if (NR != 128) { print $0}}' >> $TMP/${cpt}.cpt
	egrep '^B|^N|^F' gmt_cpts/${cpt}.cpt >> $TMP/${cpt}.cpt
	mv -f $TMP/${cpt}.cpt gmt_cpts
done < $TMP/hinge.lis
mv -f $TMP/cpt_strings.txt .
rm -rf $TMP
echo "Folder with new cpts is $DIR/gmt_cpts"
echo "The strings for src/gmt_cpt_masters.h can be found at $DIR/cpt_strings.txt"
