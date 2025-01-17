#!/bin/csh -f

#__________________________________
# makeMasterUda_index:
# This script generates an index.xml file from 
# a series of udas.  
#_________________________________
if( $#argv < 1 ) then
  echo "makeMasterUda_index.csh < list of uda files >"
  echo "   To use:"
  echo ""
  echo "   mkdir <masterUda>"
  echo "   makeMasterUda_index.csh uda.000  uda.001 uda.00N"
  exit(1)
endif

set masterUda = "masterUda"
set rootPath = `dirname $0`

if ( -l $0 ) then
  set rootPath  = `readlink $0 | xargs dirname`  # if you're using a symbolic link to script
endif

set path = ($rootPath $path)

#echo $path
set udas = ($argv[*])    # make sure you remove the last / from any entry

#__________________________________
# bulletproofing
set tmp = (`which makeCombinedIndex.sh` )
if ( $status ) then
  echo "ERROR:  makeMasterUda_index.csh:  couldn't find the script makeCombinedIndex.sh it must be in your path"
  exit
endif

if( ! -e $masterUda ) then
  echo "ERROR: makeMasterUda.csh: can't find the directory (masterUda)"
  echo " Create the directory and try again."
  exit
endif

if( ! -e $masterUda ) then
  echo "ERROR: makeMasterUda: can't find the directory (masterUda)"
  exit
endif

echo ""
echo "---------------------------------------"
foreach X ($udas[*])
  echo "Passing $X through bulletproofing section"
  
  # does each uda exist
  if (! -e $X ) then
    echo "ERROR: makeMasterUda: can't find the uda $X"
    `pwd`
    exit
  endif
  
  # does each index.xml exist
  if (! -e $X/index.xml ) then
  echo "Working on $X " 
    echo "______________________________________________________________"
    echo "ERROR: makeMasterUda: can't find the file $X/index.xml"
    echo "                   Do you want to continue"
    echo "                             Y or N"
    echo "______________________________________________________________"
    set ans = $<

    if ( $ans == "n" || $ans == "N" ) then
      exit
    endif
  endif
end

#__________________________________
# Look for difference in the variable lists between the udas
# and warn the users if one is detected
echo ""
echo "---------------------------------------"
echo "Looking for differences in the variable lists"

mkdir ~/.scratch  >&/dev/null

set io_type = "UDA"

foreach X ( $udas[*] )
  set here = `basename $X`
  grep variable $X/index.xml > ~/.scratch/$here

  # check if io type is uda or PIDX
  if (`grep -c "<outputFormat>PIDX</outputFormat>" "$X/index.xml"`) then
    set io_type="PIDX"
  endif
end

echo $io_type

set n = $#argv    # counters
@ c  = 1
@ cc = 2

while ( $c != $n)  
  set X = `basename $udas[$c]`
  set Y = `basename $udas[$cc]`
  
  #only look for differences if both index.xml files have a variable list
  set ans1 = `grep -c variables ~/.scratch/$X`
  set ans2 = `grep -c variables ~/.scratch/$Y` 
  
  if ($ans1 == "2" && $ans2 == "2") then
    diff -B ~/.scratch/$X ~/.scratch/$Y >& /dev/null
    if ($status != 0 ) then
      echo "  more Difference in the variable list detected between $X/index.xml and $Y/index.xml"
      sdiff -s -w 170 ~/.scratch/$X ~/.scratch/$Y
    endif
  endif
  @ c  = $c + 1
  @ cc = $cc + 1
end

#__________________________________
#  copy uda[1]/input.xml* to masterUda
echo ""
echo "__________________________________"
echo "Copying $udas[1]/input.xml and input.xml.orig to $masterUda"
 
if( ! -e $masterUda/input.xml ) then
  cp $udas[1]/input.xml $masterUda
endif

if( ! -e $masterUda/input.xml.orig ) then
  cp $udas[1]/input.xml.orig $masterUda
endif

#__________________________________
# copy the index.xml file from uda[1]
# remove all the timestep data
echo ""
echo "---------------------------------------"
echo "Creating the base index file from the $udas[1]"

cat $udas[1]/index.xml | sed /"timestep "/d > "$masterUda/index.tmp"

# remove   </timesteps> & </Uintah_DataArchive>
sed /"\/timesteps"/,/"\/Uintah_DataArchive"/d <"$masterUda/index.tmp" > "$masterUda/index.xml"

# add the list of timesteps to index.xml
makeCombinedIndex.sh "$udas" >> "$masterUda/index.xml"

# generate all gidx
if($io_type == "PIDX") then
  rm *.gidx
  makeCombinedGIDX.sh "$udas"
endif

# put the xml tags back
echo  "  </timesteps>" >> "$masterUda/index.xml"
echo  "</Uintah_DataArchive>" >> "$masterUda/index.xml"


#__________________________________
#  prepend the udas with "../"
#  This assumes that the udas are in the parent
#  directory above masterUda  

sed -i 's#<timestep href="#<timestep href="..\/#g' "$masterUda/index.xml"



#__________________________________
# cleanup
/bin/rm -rf ~/.scratch "$masterUda/index.tmp"
exit


