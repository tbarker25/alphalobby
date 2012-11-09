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

CFLAGS+= -Iinclude -I$(BUILD_DIR) -DUNICODE -DNO_STRICT
CFLAGS+= -D_WIN32_IE=0x0600 -D_WIN32_WINNT=0x0600 -DWINVER=0x0600 -DWIN32_LEAN_AND_MEAN

ICONS:=$(addsuffix .png, $(addprefix icons/, alphalobby battle_ingame battle_pw battle_ingame_pw battle_full battle_ingame_full battle_pw_full battle_ingame_pw_full))
ICONS+= null:
ICONS+=$(addsuffix .png, $(addprefix icons/, user_unsync user_away user_unsync_away user_ignore user_unsync_ignore user_away_ignore user_unsync_away_ignore closed open host spectator host_spectator ingame rank0 rank1 rank2 rank3 rank4 rank5 rank6 rank7 connecting battlelist replays options split_vert split_horz split_corner1 split_corner2 split_rand))


ICONS+=$(wildcard icons/flags/*)
ICONS+= null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null:

FILES1:=gzip.o client.o downloadtab.o chat_window.o client_message.o data.o alphalobby.o common.o sync.o settings.o wincommon.o userlist.o battlelist.o battleroom.o downloader.o imagelist.o layoutmetrics.o dialogboxes.o chat.o md5.o res.o countrycodes.o battletools.o spring.o usermenu.o messages.o user.o battle.o

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
	windres $< $@

$(BUILD_DIR)/%.o : src/%.c $(BUILD_DIR)/icons.h src/user.h src/battle.h src/data.h
	$(CC) -c $< -o $@ $(CFLAGS)

clean :
	rm -rf $(BUILD_DIR)

$(BUILD_DIR):
	mkdir $@
