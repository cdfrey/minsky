#! /bin/sh

here=`pwd`
if test $? -ne 0; then exit 2; fi
tmp=/tmp/$$
mkdir $tmp
if test $? -ne 0; then exit 2; fi
cd $tmp
if test $? -ne 0; then exit 2; fi

fail()
{
    echo "FAILED" 1>&2
    cd $here
    chmod -R u+w $tmp
    rm -rf $tmp
    exit 1
}

pass()
{
    echo "PASSED" 1>&2
    cd $here
    chmod -R u+w $tmp
    rm -rf $tmp
    exit 0
}

trap "fail" 1 2 3 15

# insert ecolab script code here
# use \$ in place of $ to refer to variable contents
# exit 0 to indicate pass, and exit 1 to indicate failure
cat >input.tcl <<EOF
proc bgerror x {
  puts \$x
  exit 1
}

source $here/test/assert.tcl
proc afterMinskyStarted {} {uplevel #0 {
 addPlot
 canvas.mouseUp 300 200
 findObject PlotWidget
 set item minsky.canvas.item
 set x [\$item.x]
 set y [\$item.y]
 set w [\$item.width]
 set h [\$item.height]

 canvas.mouseDown [expr \$x+0.5*\$w+2] [expr \$y+0.5*\$h+2]
 canvas.mouseUp [expr \$x+\$w]  [expr \$y+\$h]

 assert "abs([expr 1.5*\$w]-[minsky.canvas.item.width])<5"
 assert "abs([expr 1.5*\$h]-[minsky.canvas.item.height])<5"

 tcl_exit

}}
EOF

$here/gui-tk/minsky input.tcl
if test $? -ne 0; then fail; fi

pass
