# RB-23 Texture Studio acceptance

Source commit `c6bf481e28ae0e5ca89ba5b70c24c92d7048670b`

Release package SHA-256 `ec58fbd8612f189e0ceb0d100c1eeea333276856783909a1d1d7062be301a8bb`

Windows passed all 20 native scenarios. The packaged editor tests passed on Windows, Linux and Linux SDL2. QuakeSpasm, Yamagi Quake II, OpenArena, Steam Half-Life and FTE loaded the compiled fixture materials. Graph, image and ZIP parsers each ran for 7,200 seconds with address and undefined-behavior sanitizers. The editor recovered from decoder timeout and renderer restart. The release verifier binds the source and six archives.
