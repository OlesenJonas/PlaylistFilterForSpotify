place glfw and Crypto++ libs here

For Crypto++ on Windows:
Open Solution in VS and change code gen setting to /M(D)D before compiling cryptest as Debug/Relase (Not DLL-Import)
Rename debug .lib to cryptlibDebug.lib (optionally including .pdb file)