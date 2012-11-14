LDFLAGS:=-lwinhttp -lwsock32 -lcomctl32 -lgdi32 -luser32 -lkernel32 -lshell32 -lmsvcrt -lShlwapi -lzlib1 -ldevil -lshell32 -Llib
CFLAGS:= -ffast-math -fshort-enums -std=gnu99 -march=i686 -fplan9-extensions

ifdef VERSION 
	CFLAGS+= -DVERSION=$(VERSION) -DNDEBUG
	BUILD_DIR:=release
	CFLAGS+= -s -Os -mwindows
else
	BUILD_DIR:=debug
	CFLAGS+= -g3 -O0  -Wall -Werror -Wno-unknown-pragmas
endif

WIN_CFLAGS:=-D_WIN32_IE=0x0600 -D_WIN32_WINNT=0x0600 -DWINVER=0x0600 -DWIN32_LEAN_AND_MEAN -DUNICODE -DNO_STRICT
CFLAGS+= -Iinclude -I$(BUILD_DIR) $(WIN_CFLAGS)

ICONS:=$(addsuffix .png, $(addprefix icons/, alphalobby battle_ingame battle_pw battle_ingame_pw battle_full battle_ingame_full battle_pw_full battle_ingame_pw_full))
ICONS+= null:
ICONS+=$(addsuffix .png, $(addprefix icons/, user_unsync user_away user_unsync_away user_ignore user_unsync_ignore user_away_ignore user_unsync_away_ignore closed open host spectator host_spectator ingame rank0 rank1 rank2 rank3 rank4 rank5 rank6 rank7 connecting battlelist replays chat options downloads split_vert split_horz split_corner1 split_corner2 split_rand))


ICONS+=$(wildcard icons/flags/*)
ICONS+= null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null:

FILES1:=alphalobby.o battle.o battlelist.o battleroom.o battletools.o chat.o chat_window.o client.o client_message.o common.o countrycodes.o dialogboxes.o downloader.o downloadtab.o gzip.o imagelist.o layoutmetrics.o md5.o messages.o res.o settings.o spring.o sync.o user.o userlist.o usermenu.o wincommon.o channellist.o

FILES=$(addprefix $(BUILD_DIR)/, $(FILES1))

all: $(BUILD_DIR)/alphalobby.exe

release/alphalobby.exe: $(BUILD_DIR) $(FILES)
	$(CC) $(FILES) -o $@-big $(CFLAGS) $(LDFLAGS)
	upx $@-big -o$@ -qq --ultra-brute

debug/alphalobby.exe: $(BUILD_DIR) $(FILES)
	$(CC) $(FILES) -o $@ $(CFLAGS) $(LDFLAGS)

$(BUILD_DIR)/icons.h: $(BUILD_DIR)/rgba2c.exe
	montage $(ICONS) -background transparent -tile x1 -geometry 16x16 -depth 8 rgba:- | $(BUILD_DIR)/rgba2c.exe > $@

$(BUILD_DIR)/rgba2c.exe : src/rgba2c.c
	$(CC) -std=c99 $< -o $@

$(BUILD_DIR)/res.o: src/res.rc
	windres $< $@ $(WIN_CFLAGS)

$(BUILD_DIR)/%.o : src/%.c $(BUILD_DIR)/icons.h src/user.h src/battle.h src/data.h
	$(CC) -c $< -o $@ $(CFLAGS)

clean :
	rm -rf $(BUILD_DIR)

$(BUILD_DIR):
	mkdir $@
