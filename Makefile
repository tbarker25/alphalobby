LDFLAGS:=-lwinhttp -lwsock32 -lcomctl32 -lgdi32 -luser32 -lkernel32 -lShlwapi -lz -ldevil -lshell32 -Llib
CFLAGS:= -ffast-math -fshort-enums -std=gnu99 -march=i686 -fplan9-extensions -mno-ms-bitfields
CC:=gcc

ifdef VERSION
	CFLAGS+= -DVERSION=$(VERSION) -DNDEBUG
	BUILD_DIR:=release
	LDFLAGS+= -lmsvcrt
	CFLAGS+= -s -Os -mwindows -flto -fomit-frame-pointer
else
	CFLAGS+= -DVERSION=$(VERSION)
	BUILD_DIR:=debug
	LDFLAGS+= -lmsvcrt
	CFLAGS+= -g3 -O0
endif

WARNINGS:=-Wall -Werror -Wno-unknown-pragmas -Wclobbered -Wempty-body -Wignored-qualifiers -Wmissing-parameter-type -Woverride-init -Wtype-limits -Wuninitialized -Wsuggest-attribute=pure -Wsuggest-attribute=const -Wsuggest-attribute=noreturn -Wmissing-format-attribute -Wextra
WIN_CFLAGS:=-D_WIN32_IE=0x0600 -D_WIN32_WINNT=0x0600 -DWINVER=0x0600 -DWIN32_LEAN_AND_MEAN -DUNICODE -DSTRICT -DNO_OLDNAMES
CFLAGS+= -Iinclude -I$(BUILD_DIR) $(WIN_CFLAGS) $(WARNINGS)


ICONS:=$(addsuffix .png, $(addprefix icons/, alphalobby battle_ingame battle_pw battle_ingame_pw battle_full battle_ingame_full battle_pw_full battle_ingame_pw_full))
ICONS+= null:
ICONS+=$(addsuffix .png, $(addprefix icons/, user_unsync user_away user_unsync_away user_ignore user_unsync_ignore user_away_ignore user_unsync_away_ignore closed open host spectator host_spectator ingame rank0 rank1 rank2 rank3 rank4 rank5 rank6 rank7 connecting battlelist replays chat options downloads split_vert split_horz split_corner1 split_corner2 split_rand))


ICONS+=$(wildcard icons/flags/*)
ICONS+= null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null: null:

FILES1:=battle.o battlelist.o battleroom.o channellist.o chat.o chat_window.o common.o countrycodes.o dialogboxes.o downloader.o downloadtab.o gzip.o host_relay.o host_self.o host_spads.o host_springie.o iconlist.o layoutmetrics.o mainwindow.o md5.o messages.o mybattle.o minimap.o res.o settings.o spring.o sync.o tasserver.o user.o userlist.o usermenu.o wincommon.o

FILES=$(addprefix $(BUILD_DIR)/, $(FILES1))

all: $(BUILD_DIR)/alphalobby.exe

release/alphalobby.exe: $(BUILD_DIR) $(FILES)
	$(CC) $(FILES) -o $@-big $(CFLAGS) $(LDFLAGS)
	rm $@
	upx $@-big -o$@ -qq --ultra-brute

debug/alphalobby.exe: $(BUILD_DIR) $(FILES)
	$(CC) $(FILES) -o $@ $(CFLAGS) $(LDFLAGS)

$(BUILD_DIR)/icons.h: $(BUILD_DIR)/rgba2c.exe
	montage $(ICONS) -background transparent -tile x1 -geometry 16x16 -depth 8 rgba:- | $(BUILD_DIR)/rgba2c.exe > $@

$(BUILD_DIR)/rgba2c.exe : src/rgba2c.c
	$(CC) -std=c99 $< -o $@

$(BUILD_DIR)/res.o: src/res.rc
	windres $< $@ $(WIN_CFLAGS)

$(BUILD_DIR)/%.o : src/%.c $(BUILD_DIR)/icons.h src/user.h src/battle.h Makefile
	$(CC) -c $< -o $@ $(CFLAGS)

clean :
	rm -rf $(BUILD_DIR)

$(BUILD_DIR):
	mkdir $@
