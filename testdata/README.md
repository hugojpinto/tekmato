# Test disc images

The regression (`scripts/regression.sh`) reads these images and never writes them: every test that changes a disc works on a copy in a temporary directory. They are © Tektronix, Inc., from public archives, and are kept here for preservation, research and testing. They are not covered by tekmato's licence (see [NOTICE.md](../NOTICE.md)).

| Directory | What | Source |
|---|---|---|
| `dos50/` | DOS/50 V1.0 system disc (`System.imd`), the 8085 and 8086 emulator discs, PROM programmer software, the 1985 diagnostics (`8550_DIAG.IMD`) and version 2.4 (`062-5412-08_8501_1err_DOS50.imd`) | bitsavers, `bits/Tektronix/`, as kept in the [8550 emulator](https://github.com/hugojpinto/tektronix-8550-emulator)'s `sources/bitsavers/disks/` |
| `dos50/System_Fixed.img` | `System.imd` with its damaged HELP file repaired | the 8550 emulator's `disks/` (how it was repaired: its `disks/README.md`) |
| `tekdos/` | TEKDOS 3.3 (6800) and 3.1 (6500) system discs, with their imaging logs | bitsavers, `bits/Tektronix/8002/`, as kept in the [8002 emulator](https://github.com/hugojpinto/tektronix-8002-emulator)'s `sources/bitsavers/disks/` |
| `tekdos/Tek8002_Belgium_Amusements/` | the 30 working discs of bitsavers' `Tek8002_Belgium_Amusements` collection, with two text notes (`Tek8002_1a.img` was photographed with a printout of its directory; `Tek8002_1b.img` is a disc TEKDOS's FORMAT left) | the same |

SHA-256:
```
3286410119460602891f876346fbf4c056d4dae9f7c7f1f1d525997c2e6fe2d7  ./dos50/062-4641-03_8300E06_8085_Emulator_V2.1_DOS50.imd
5138d55181c75cf105b515832a0987c3c399f8df2511f37a973aed1bf01139cd  ./dos50/062-5412-08_8501_1err_DOS50.imd
b5c03df8c1c7235a757e6da1502b279bf01fec34eed2011816d06066f7a5e1ce  ./dos50/062-6036-01_8550F30_prom_programmer_2.1_1982.imd
1b51361885f87c036d056866c44fb36759b91ef911e256020cf05e52c9c72adb  ./dos50/8550_8086.IMD
58c2ef91d4bedd2171cb1c6c08c77bf30af9f922c049b48f760c96af8645aa60  ./dos50/8550_DIAG.IMD
6af83059952cdba378f398da3f4c70f81eac2321404615f6a616e5eaad25a21f  ./dos50/System_Fixed.img
22fce3e4fd0ea247036005f47a9b0125018a15b29b1a0aff936ec19cfa19c270  ./dos50/System.imd
9bab80b112f454da7f90c7295e593e3014d3e88b446573054b1af0c9eef86769  ./README.new
6f33708b29bb62c09a9196a1033a615bf454f3245d1e45215eb23a3f94512ee4  ./tekdos/Tek8002_Belgium_Amusements/Tek8002_10.img
8c00bdb669018e70765a7b0d4e201a346999343e22f6118f7c6f5cd07b4ae847  ./tekdos/Tek8002_Belgium_Amusements/Tek8002_11.img
ae8b2b0a9d85089ec3bfd10f41d597996d3573539c5b5a47f5c3f17157a44a5d  ./tekdos/Tek8002_Belgium_Amusements/Tek8002_12.img
ee5982e8aebbea7559d1c3ca4b79ba7e56bac57aa9a89675c437fc67232891a4  ./tekdos/Tek8002_Belgium_Amusements/Tek8002_13.img
0cde5d5adcf147f05c1af916747642931c2e9a30203c16140a712876c57cc71c  ./tekdos/Tek8002_Belgium_Amusements/Tek8002_14.img
8f359d71a868264a0b28e0b0a6170a801c005f1515801ae3088cfc559aeb726a  ./tekdos/Tek8002_Belgium_Amusements/Tek8002_15.img
82eaf880c1d492b75ec2a4d720aaa569c5b60231a0f3e5b29a0af62f3ddee423  ./tekdos/Tek8002_Belgium_Amusements/Tek8002_16.img
5ae8e3551535f0ee1f2195cfb9134001660d69c305b5367424159bb8744efe25  ./tekdos/Tek8002_Belgium_Amusements/Tek8002_17a.img
3f8e43b7c66ea5ec2f51ea8e05c99dea8517bb45cad94484631ac3c72b61ee6d  ./tekdos/Tek8002_Belgium_Amusements/Tek8002_17b.img
975994662bffcacf5fad1e812a132a596985f53405ee93bf4a4792c6026d52a5  ./tekdos/Tek8002_Belgium_Amusements/Tek8002_18.img
f07bbcbcc4fc13ccc0af6ccc601950c4553afe71d347643345ae740f98ac5c35  ./tekdos/Tek8002_Belgium_Amusements/Tek8002_19.img
fc7e93d058ee05cfc3e3d14bf25b61145e91c6d206e751550b128d996c7eee35  ./tekdos/Tek8002_Belgium_Amusements/Tek8002_1a.img
5d90fb51158bc140a6f92272aeb8b8b9ddbf051807280c0e6974a974ebc8e0b9  ./tekdos/Tek8002_Belgium_Amusements/Tek8002_1b.img
ed2dce303a94c9caa68972d796bcbad29e4c620f199261fe033cad0aa97645d7  ./tekdos/Tek8002_Belgium_Amusements/Tek8002_2.img
5af0f34a59b7fb03814c7f2f2debf03aeb8eb99f138dcc4a38a249d8e5a43efe  ./tekdos/Tek8002_Belgium_Amusements/Tek8002_20.img
ad0b36810c50c203cc57e0d00d2242a068dfc20ea55fbf22c158e9f65fc495ec  ./tekdos/Tek8002_Belgium_Amusements/Tek8002_21.img
1ac671fa6a8c6ac8acb1e1d9d5e6bb5475397234ad8ab2a3b9428fed8d630fa7  ./tekdos/Tek8002_Belgium_Amusements/Tek8002_22.img
d66b8c583d27c6f8ad8cbed32d45f4f3b8bb1c198b401fc35e91bef601b7ed9f  ./tekdos/Tek8002_Belgium_Amusements/Tek8002_22.txt
137d2c03d1b19f51e9aee5d05e363ead2e802179a4e9a3b4194d6b4a30d3b435  ./tekdos/Tek8002_Belgium_Amusements/Tek8002_23a.img
dad2765c0cb409dade923d0bf514dbf1f2901fa80b09fd95dca8c70818d21eaa  ./tekdos/Tek8002_Belgium_Amusements/Tek8002_23b.img
a3c9f470a56b07516fd92d84264429f3508257de0701c0549fe1edcac63feb44  ./tekdos/Tek8002_Belgium_Amusements/Tek8002_24a.img
6e62fb703b8a646fded805473a5acc173fba6334049213486a1c4ca812cc2f5d  ./tekdos/Tek8002_Belgium_Amusements/Tek8002_24a.txt
0e0437f84eae4316f04232bc0cd0b3a23fb9ae0f0615c86f4e8abded5e429215  ./tekdos/Tek8002_Belgium_Amusements/Tek8002_24b.img
f34f04cfc9c2bbeb5a5a6c6a4da62afe94f9c6d9bfc4ded883ca2bc118b33ea9  ./tekdos/Tek8002_Belgium_Amusements/Tek8002_25.img
9689b28e14447260f70aa0fdc5156f17c214b614d146f513a0bbad8654f94bd3  ./tekdos/Tek8002_Belgium_Amusements/Tek8002_26.img
1d98a32d2de21214219660cdbde82b7324a2f8057509780d42075aae42a5e664  ./tekdos/Tek8002_Belgium_Amusements/Tek8002_3.img
27b02de5aea8665308b727d4e8a5fcdfac57db2290510c1b227f21a20aa33a88  ./tekdos/Tek8002_Belgium_Amusements/Tek8002_4.img
12a272140a12a84435fb27f5a049122e8c95b1a1e1d6617e4654cf20b3e8c3fd  ./tekdos/Tek8002_Belgium_Amusements/Tek8002_5.img
6c9df7da3b995ff0daacf205d4ad93a66cd8f79e995878c6a89e992de9453f94  ./tekdos/Tek8002_Belgium_Amusements/Tek8002_6a.img
07c358e088d909fa6435cd631067dfcc0f675ca6635c88a07941ec5a308ed8f7  ./tekdos/Tek8002_Belgium_Amusements/Tek8002_7a.img
77c12b866a27df99d2221df278dde71d5c1868acc0f03cb4530e69c821a7b5d9  ./tekdos/Tek8002_Belgium_Amusements/Tek8002_8.img
6b3b804faeac6a1dfada553874337fe9e566909377f755218dcbddbfb4a3fb79  ./tekdos/Tek8002_Belgium_Amusements/Tek8002_9.img
56aad3068ea2331bb75ca45e36937705aa5c14e64dd80c56b2b56e9282b05444  ./tekdos/TekDOS_3.3_6800_cpy1_-_Imaging_Log.txt
5b5a9b3a83c992b221acd28efd2a75e292b9e758f330d2a7964cdc411c929421  ./tekdos/TekDOS_3.3_6800_cpy1.img
b85020d91b8672d92cb96427c5066543fdaab5e6e95dc00c6e0f905f1b620887  ./tekdos/TekDOS_3.3_6800_cpy2_-_Imaging_Log.txt
8a897730a66dfff4720f440074bb09cdfacf052dbad903f46eb3b24526762bec  ./tekdos/TekDOS_3.3_6800_cpy2.img
e2e6ea2e10344c0a54088d66c22f44f6e5cb5009e7f283d810ae43d968f615cc  ./tekdos/TekDOS_version_3.1_6500_1_copy/tekdos_3.1_6500_copy.img
```
