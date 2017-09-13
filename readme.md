[![Build Status](https://travis-ci.org/flamewing/sorcery-xtractobb.svg?branch=master)](https://travis-ci.org/flamewing/sorcery-xtractobb)
[![Coverity Scan Build Status](https://scan.coverity.com/projects/13717/badge.svg)](https://scan.coverity.com/projects/13717)

Inkle Sorcery! Extractor
========================
This software is capable of extracting the Android OBB files used in the Inkle Sorcery! series. It is possible that other Inkle games can also be extracted with this tool, but I did not test it.

To compile this tool you need a C++14-compatible compiler (GCC 5.4 is enough), as well as Boost. When you meet the requirements, run the "build.sh" script and the "xtractobb" executable will be created. Its usage is:

    xtractobb <obbfile> <outputdir>

The tool will scan all files packed into the OBB and extract them into the output directory. It will also create a "SorceryN-Reference.json" file that stitches together "SorceryN.json" with the contents of "SorceryN.inkcontent".

Also provided is a "xtract_all_obbs.sh" which will extract all Sorcery! OBBs and link all JSON files for easier browsing.

TODO
====
- [ ] Create a OBB directory abstraction layer;
- [ ] Determine main story filename using "StoryFilename" and "[StoryFilename]PartNumber" properties from "Info.plist" file instead of hard-coding;
- [ ] Use "indexed-content/filename" attribute in story file to determine inkcontent file instead of hard-coding;
- [ ] Support for other Inkle games;
- [ ] Support for generating new OBB files;
- [ ] Decompile the reference file into [Ink script](https://github.com/inkle/ink);
- [ ] Use a different JSON tokenizer.