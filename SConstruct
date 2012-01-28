env = Environment(tools=['mingw'])
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

cflags += ' -I' + variant_dir

env.VariantDir(variant_dir, 'src', duplicate=0)

rgba2c = env.Program(variant_dir + '/rgba2c', [variant_dir + '/rgba2c.c'], CCFLAGS=cflags)


ICONS=' alphalobby battle_ingame battle_pw battle_ingame_pw battle_full battle_ingame_full battle_pw_full battle_ingame_pw_full'
ICONS+=' null: user_unsync user_away user_unsync_away user_ignore user_unsync_ignore user_away_ignore user_unsync_away_ignore'
ICONS+=" closed open host spectator host_spectator ingame"
ICONS+=" rank0 rank1 rank2 rank3 rank4 rank5 rank6 rank7"
ICONS+=" close horizontal_split vertical_split edge_split corner_split random_split"
ICONS+=" flags/*"
ICONS2 = ' '.join('icons/' + f + '.png' if f != 'null:' else 'null:' for f in ICONS.split())
ICONS2 += ' null:' * 48

Depends(variant_dir + '/icons.h', rgba2c)
icons = env.Command(variant_dir + '/icons.h', [], 'ImageMagick\\montage ' + ICONS2 + ' -background transparent -tile x1 -geometry 16x16 -depth 8 rgba:- | ' + variant_dir + '\\rgba2c.exe' + ' > $TARGET')

files = Split('client.c client_message.c data.c alphalobby.c common.c sync.c settings.c wincommon.c userlist.c battlelist.c battleroom.c downloader.c imagelist.c layoutmetrics.c dialogboxes.c chat.c md5.c res.o countrycodes.c battletools.c spring.c usermenu.c')
files = [variant_dir + '/' + s for s in files]

resources = env.RES(variant_dir + '/res.rc')


Depends(variant_dir + '/alphalobby', icons)
alphalobby = env.Program(variant_dir + '/alphalobby', [files, resources], LIBS=libraries, LIBPATH=libpath, LINKFLAGS=cflags, CCFLAGS=cflags, CPPDEFINES=cppdefines)


if version != '(development build)':
	Command(variant_dir + '/alphalobby-' + version + '.exe', alphalobby, 'upx $SOURCE -qq --brute -f -o $TARGET')
