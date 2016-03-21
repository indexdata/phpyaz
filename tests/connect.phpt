--TEST--
yaz_connect
--SKIPIF--
<?php if (!extension_loaded("yaz")) print "skip"; ?>
--FILE--
<?php
$z = yaz_connect("z3950.indexdata.com/gils");
yaz_search($z, "rpn", "computer");
yaz_wait();
echo yaz_errno($z) . "\n";
echo yaz_hits($z, $res) . "\n";
echo $res[0]['subquery.term'] . ":" . $res[0]['count'] . "\n";
yaz_search($z, "rpn", "@attr 1=99 computer");
yaz_wait();
echo yaz_errno($z) . "\n";
echo yaz_hits($z) . "\n";
if (is_resource($z)) {
   echo "1\n";
} else {
   echo "0\n";
}
yaz_close($z);
if (is_resource($z)) {
   echo "1\n";
} else {
   echo "0\n";
}
--EXPECT--
0
3
computer:3
114
0
1
0
