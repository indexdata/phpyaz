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
echo yaz_hits($z) . "\n";
yaz_search($z, "rpn", "@attr 1=99 computer");
yaz_wait();
echo yaz_errno($z) . "\n";
echo yaz_hits($z) . "\n";
--EXPECT--
0
3
114
0
