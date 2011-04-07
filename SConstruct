
libraries = Split('winhttp wsock32 comctl32 gdi32 user32 kernel32 shell32 msvcrt Shlwapi zlib1')
libpath = '.'
cflags = '-ffast-math -fshort-enums -std=gnu99 -march=i686'

version = ARGUMENTS.get('VERSION', '(development build)')
cppdefines = {'UNICODE' : 1, 'VERSION' : version}

if version == '(development build)':
	variant_dir = 'debug'
	cflags += ' -g -O0'
else:
	cppdefines['NDEBUG'] = 1
	variant_dir = 'release'
	cflags += ' -s -Os -mwindows'

VariantDir(variant_dir, 'src', duplicate=0)

files = Split('client.c client_message.c data.c alphalobby.c common.c sync.c settings.c wincommon.c userlist.c battlelist.c battleroom.c downloader.c imagelist.c layoutmetrics.c dialogboxes.c chat.c md5.c res.o countrycodes.c battletools.c spring.c usermenu.c')
files = [variant_dir + '/' + s for s in files]

resources = RES(variant_dir + '/res.rc')

prog = Program(variant_dir + '/alphalobby', [files, resources], LIBS=libraries, LIBPATH=libpath, LINKFLAGS=cflags, CCFLAGS=cflags, CPPDEFINES=cppdefines)

if version != '(development build)':
	Command(variant_dir + '/alphalobby-' + version + '.exe', prog, 'upx $SOURCE -qq --brute -f -o $TARGET')
