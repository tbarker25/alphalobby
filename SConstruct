
libraries = Split('winhttp wsock32 comctl32 gdi32 user32 kernel32 shell32 msvcrt Shlwapi zlib1')
libpath = '.'
cflags = '-ffast-math -fshort-enums -std=gnu99 -DUNICODE -march=i686'
cppdefines = {}

if ARGUMENTS.get('NDEBUG', 0) == 0:
	variant_dir = 'debug'
	cflags += ' -g -O0'
else:
	cppdefines['NDEBUG'] = 1
	variant_dir = 'release'
	cflags += ' -s -Os'

VariantDir(variant_dir, 'src', duplicate=0)

files = Split('client.c client_message.c data.c alphalobby.c common.c sync.c settings.c wincommon.c userlist.c battlelist.c battleroom.c downloader.c imagelist.c layoutmetrics.c dialogboxes.c chat.c md5.c res.o countrycodes.c battletools.c spring.c usermenu.c')
files = [variant_dir + '/' + s for s in files]

resources = RES(variant_dir + '/res.rc')

Program(variant_dir + '/alphalobby', [files, resources], LIBS=libraries, LIBPATH=libpath, CCFLAGS=cflags, CPPDEFINES=cppdefines)