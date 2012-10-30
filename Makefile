BUILD_DIR:=debug
LDFLAGS:=-lwinhttp -lwsock32 -lcomctl32 -lgdi32 -luser32 -lkernel32 -lshell32 -lmsvcrt -lShlwapi -lzlib1 -ldevil -lshell32 -Llib
CFLAGS:= -ffast-math -fshort-enums -std=gnu99 -march=i686
CFLAGS+= -Iinclude -I$(BUILD_DIR) -DUNICODE=1
#cppdefines = {'UNICODE' : 1}
#if version:
#	cppdefines['VERSION'] = version

#if version:
#	cppdefines['NDEBUG'] = 1
#	variant_dir = 'release'
#	cflags += ' -s -Os -mwindows'
#else:
#	variant_dir = 'debug'
#	cflags += ' -g -O0  -Wall -Werror'

ICONS:=$(addsuffix .png, $(addprefix icons/, alphalobby battle_ingame battle_pw battle_ingame_pw battle_full battle_ingame_full battle_pw_full battle_ingame_pw_full))
ICONS+= null:
ICONS+=$(addsuffix .png, $(addprefix icons/, user_unsync user_away user_unsync_away user_ignore user_unsync_ignore user_away_ignore user_unsync_away_ignore closed open host spectator host_spectator ingame split_vert split_horz split_corner1 split_corner2 split_rand))
ICONS+=$(wildcard icons/flags/*)
ICONS+= null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null:

#if version:
	#Command(variant_dir + '/alphalobby-' + version + '.exe', alphalobby, 'upx $SOURCE -qq --brute -f -o $TARGET')

FILES1:=gzip.o client.o downloadtab.o chat_window.o client_message.o data.o alphalobby.o common.o sync.o settings.o wincommon.o userlist.o battlelist.o battleroom.o downloader.o imagelist.o layoutmetrics.o dialogboxes.o chat.o md5.o res.o countrycodes.o battletools.o spring.o usermenu.o

FILES=$(addprefix $(BUILD_DIR)/, $(FILES1))

all: $(BUILD_DIR)/alphalobby

$(BUILD_DIR)/alphalobby: $(BUILD_DIR) $(FILES)
	$(CC) $(FILES) -o $@ $(CFLAGS) $(LDFLAGS)

$(BUILD_DIR)/icons.h: $(BUILD_DIR)/rgba2c.exe
	montage $(ICONS) -background transparent -tile x1 -geometry 16x16 -depth 8 rgba:- | $(BUILD_DIR)/rgba2c.exe > $@

$(BUILD_DIR)/rgba2c.exe : src/rgba2c.c
	$(CC) -std=c99 $< -o $@

$(BUILD_DIR)/res.o: src/res.rc
	windres $< $@

$(BUILD_DIR)/%.o : src/%.c $(BUILD_DIR)/icons.h
	$(CC) -c $< -o $@ $(CFLAGS)

clean :
	rm -rf $(BUILD_DIR)

$(BUILD_DIR):
	mkdir $@
