; system/file.r
(#{C3A4C3B6C3BC} == read %fixtures/umlauts-utf8.txt)
("äöü" == read/string %fixtures/umlauts-utf8.txt)
(["äöü"] == read/lines %fixtures/umlauts-utf8.txt)
(#{EFBBBFC3A4C3B6C3BC} == read %fixtures/umlauts-utf8bom.txt)
("äöü" == read/string %fixtures/umlauts-utf8bom.txt)
(["äöü"] == read/lines %fixtures/umlauts-utf8bom.txt)
(#{FFFEE400F600FC00} == read %fixtures/umlauts-utf16le.txt)
("äöü" == read/string %fixtures/umlauts-utf16le.txt)
(["äöü"] == read/lines %fixtures/umlauts-utf16le.txt)
(#{FEFF00E400F600FC} == read %fixtures/umlauts-utf16be.txt)
("äöü" == read/string %fixtures/umlauts-utf16be.txt)
(["äöü"] == read/lines %fixtures/umlauts-utf16be.txt)
(#{FFFE0000E4000000F6000000FC000000} == read %fixtures/umlauts-utf32le.txt)
(#{0000FEFF000000E4000000F6000000FC} == read %fixtures/umlauts-utf32be.txt)
(block? read %./)
(block? read %fixtures/)


; These save tests were living in %mezz-save.r, but did not have expected
; outputs.  Moved here with expected binary result given by R3-Alpha.

(block? data: [1 1.2 10:20 "test" user@example.com [sub block]])

((save blank []) = #{
0A
})

((save blank data) = #{
3120312E322031303A3230202274657374222075736572406578616D706C652E
636F6D205B73756220626C6F636B5D0A
})

((save/header blank data [title: "my code"]) = #{
5245424F4C205B0A202020207469746C653A20226D7920636F6465220A5D0A31
20312E322031303A3230202274657374222075736572406578616D706C652E63
6F6D205B73756220626C6F636B5D0A
})

((save/compress blank [] true) = #{
5245424F4C205B0A202020206F7074696F6E733A205B636F6D70726573735D0A
5D0A789CE30200000B000B01000000
})

((save/compress blank data true) = #{
5245424F4C205B0A202020206F7074696F6E733A205B636F6D70726573735D0A
5D0A789C335430D433523034B0323250502A492D2E5152282D4E2D7248AD48CC
2DC849D54BCECF55882E2E4D5248CAC94FCE8EE5020049C70EF330000000
})

((save/compress blank data 'script) = #{
5245424F4C205B0A202020206F7074696F6E733A205B636F6D70726573735D0A
5D0A3634237B654A777A564444554D3149774E4C41794D6C42514B6B6B744C6C
46534B43314F4C584A4972556A4D4C63684A3155764F7A3157494C69354E556B
6A4B79552F4F6A75554341456E4844764D77414141417D
})

((save/header/compress blank data [title: "my code"] true) = #{
5245424F4C205B0A202020207469746C653A20226D7920636F6465220A202020
206F7074696F6E733A205B636F6D70726573735D0A5D0A789C335430D4335230
34B0323250502A492D2E5152282D4E2D7248AD48CC2DC849D54BCECF55882E2E
4D5248CAC94FCE8EE5020049C70EF330000000
})

((save/header/compress blank data [title: "my code"] 'script) = #{
5245424F4C205B0A202020207469746C653A20226D7920636F6465220A202020
206F7074696F6E733A205B636F6D70726573735D0A5D0A3634237B654A777A56
4444554D3149774E4C41794D6C42514B6B6B744C6C46534B43314F4C584A4972
556A4D4C63684A3155764F7A3157494C69354E556B6A4B79552F4F6A75554341
456E4844764D77414141417D
})

((save/header blank data [title: "my code" options: [compress]]) = #{
5245424F4C205B0A202020207469746C653A20226D7920636F6465220A202020
206F7074696F6E733A205B636F6D70726573735D0A5D0A789C335430D4335230
34B0323250502A492D2E5152282D4E2D7248AD48CC2DC849D54BCECF55882E2E
4D5248CAC94FCE8EE5020049C70EF330000000
})

;-- This gave an error in R3-Alpha:
;-- ** Script error: save does not allow none! for its method argument
;
;[(save/header/compress blank data [
;    title: "my code" options: [compress]
;] blank) = #{
;    ???
;}]

((save/header blank data [title: "my code" checksum: true]) = #{
5245424F4C205B0A202020207469746C653A20226D7920636F6465220A202020
20636865636B73756D3A20237B42424135424634364139354332384137363438
3036303233394546364536374246354235304144317D0A5D0A3120312E322031
303A3230202274657374222075736572406578616D706C652E636F6D205B7375
6220626C6F636B5D0A
})
