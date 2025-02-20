--TEST--
yaz_record
--SKIPIF--
<?php if (!extension_loaded("yaz")) print "skip"; ?>
--FILE--
<?php
$z = yaz_connect("z3950.indexdata.com/marc");
yaz_range($z, 1, 2);
yaz_search($z, "rpn", "computer");
yaz_syntax($z, "marc21");
yaz_wait();
echo yaz_errno($z) . ":" . yaz_error($z) . ":" . yaz_addinfo($z) . "\n";
echo yaz_hits($z) . "\n";
echo yaz_record($z, 1, "string");
echo yaz_record($z, 1, "xml");
yaz_element($z, "B");
echo yaz_record($z, 1, "xml");
print_r(yaz_record($z, 1, "array"));
--EXPECT--
0::
11
00366nam  22001698a 4500
001    11224467 
003 DLC
005 00000000000000.0
008 910710c19910701nju           00010 eng  
010    $a    11224467 
040    $a DLC $c DLC
050 00 $a 123-xyz
100 10 $a Jack Collins
245 10 $a How to program a computer
260 1  $a Penguin
263    $a 8710
300    $a p. cm.

<record xmlns="http://www.loc.gov/MARC21/slim">
  <leader>00366nam a22001698a 4500</leader>
  <controlfield tag="001">   11224467 </controlfield>
  <controlfield tag="003">DLC</controlfield>
  <controlfield tag="005">00000000000000.0</controlfield>
  <controlfield tag="008">910710c19910701nju           00010 eng  </controlfield>
  <datafield tag="010" ind1=" " ind2=" ">
    <subfield code="a">   11224467 </subfield>
  </datafield>
  <datafield tag="040" ind1=" " ind2=" ">
    <subfield code="a">DLC</subfield>
    <subfield code="c">DLC</subfield>
  </datafield>
  <datafield tag="050" ind1="0" ind2="0">
    <subfield code="a">123-xyz</subfield>
  </datafield>
  <datafield tag="100" ind1="1" ind2="0">
    <subfield code="a">Jack Collins</subfield>
  </datafield>
  <datafield tag="245" ind1="1" ind2="0">
    <subfield code="a">How to program a computer</subfield>
  </datafield>
  <datafield tag="260" ind1="1" ind2=" ">
    <subfield code="a">Penguin</subfield>
  </datafield>
  <datafield tag="263" ind1=" " ind2=" ">
    <subfield code="a">8710</subfield>
  </datafield>
  <datafield tag="300" ind1=" " ind2=" ">
    <subfield code="a">p. cm.</subfield>
  </datafield>
</record>
<record xmlns="http://www.loc.gov/MARC21/slim">
  <leader>00366nam a22001698a 4500</leader>
  <controlfield tag="001">   11224467 </controlfield>
  <controlfield tag="003">DLC</controlfield>
  <controlfield tag="005">00000000000000.0</controlfield>
  <controlfield tag="008">910710c19910701nju           00010 eng  </controlfield>
  <datafield tag="010" ind1=" " ind2=" ">
    <subfield code="a">   11224467 </subfield>
  </datafield>
  <datafield tag="040" ind1=" " ind2=" ">
    <subfield code="a">DLC</subfield>
    <subfield code="c">DLC</subfield>
  </datafield>
  <datafield tag="050" ind1="0" ind2="0">
    <subfield code="a">123-xyz</subfield>
  </datafield>
  <datafield tag="100" ind1="1" ind2="0">
    <subfield code="a">Jack Collins</subfield>
  </datafield>
  <datafield tag="245" ind1="1" ind2="0">
    <subfield code="a">How to program a computer</subfield>
  </datafield>
  <datafield tag="260" ind1="1" ind2=" ">
    <subfield code="a">Penguin</subfield>
  </datafield>
  <datafield tag="263" ind1=" " ind2=" ">
    <subfield code="a">8710</subfield>
  </datafield>
  <datafield tag="300" ind1=" " ind2=" ">
    <subfield code="a">p. cm.</subfield>
  </datafield>
</record>
Array
(
    [0] => Array
        (
            [0] => (3,leader)
            [1] => 00366nam  22001698a 4500
        )

    [1] => Array
        (
            [0] => (3,001)
        )

    [2] => Array
        (
            [0] => (3,001)(3,@)
            [1] =>    11224467 
        )

    [3] => Array
        (
            [0] => (3,003)
        )

    [4] => Array
        (
            [0] => (3,003)(3,@)
            [1] => DLC
        )

    [5] => Array
        (
            [0] => (3,005)
        )

    [6] => Array
        (
            [0] => (3,005)(3,@)
            [1] => 00000000000000.0
        )

    [7] => Array
        (
            [0] => (3,008)
        )

    [8] => Array
        (
            [0] => (3,008)(3,@)
            [1] => 910710c19910701nju           00010 eng  
        )

    [9] => Array
        (
            [0] => (3,010)
        )

    [10] => Array
        (
            [0] => (3,010)(3,  )
        )

    [11] => Array
        (
            [0] => (3,010)(3,  )(3,a)
            [1] =>    11224467 
        )

    [12] => Array
        (
            [0] => (3,040)
        )

    [13] => Array
        (
            [0] => (3,040)(3,  )
        )

    [14] => Array
        (
            [0] => (3,040)(3,  )(3,a)
            [1] => DLC
        )

    [15] => Array
        (
            [0] => (3,040)(3,  )(3,c)
            [1] => DLC
        )

    [16] => Array
        (
            [0] => (3,050)
        )

    [17] => Array
        (
            [0] => (3,050)(3,00)
        )

    [18] => Array
        (
            [0] => (3,050)(3,00)(3,a)
            [1] => 123-xyz
        )

    [19] => Array
        (
            [0] => (3,100)
        )

    [20] => Array
        (
            [0] => (3,100)(3,10)
        )

    [21] => Array
        (
            [0] => (3,100)(3,10)(3,a)
            [1] => Jack Collins
        )

    [22] => Array
        (
            [0] => (3,245)
        )

    [23] => Array
        (
            [0] => (3,245)(3,10)
        )

    [24] => Array
        (
            [0] => (3,245)(3,10)(3,a)
            [1] => How to program a computer
        )

    [25] => Array
        (
            [0] => (3,260)
        )

    [26] => Array
        (
            [0] => (3,260)(3,1 )
        )

    [27] => Array
        (
            [0] => (3,260)(3,1 )(3,a)
            [1] => Penguin
        )

    [28] => Array
        (
            [0] => (3,263)
        )

    [29] => Array
        (
            [0] => (3,263)(3,  )
        )

    [30] => Array
        (
            [0] => (3,263)(3,  )(3,a)
            [1] => 8710
        )

    [31] => Array
        (
            [0] => (3,300)
        )

    [32] => Array
        (
            [0] => (3,300)(3,  )
        )

    [33] => Array
        (
            [0] => (3,300)(3,  )(3,a)
            [1] => p. cm.
        )

)
