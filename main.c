#include <pspkernel.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspge.h>
#include <stdlib.h>

PSP_MODULE_INFO("SunsetSoul", PSP_MODULE_USER, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

#define W 480
#define H 272
#define STRIDE 512
#define FRAME_BYTES (STRIDE * H * 4)

enum { EXPLORE, DIALOGUE, BATTLE, VICTORY, GAMEOVER };

static unsigned int *draw_fb;   /* back buffer (uncached) */
static int draw_off;            /* its offset inside VRAM */

/* ---------- Home button ---------- */
static int exit_cb(int a1, int a2, void *arg) {
    sceKernelExitGame();
    return 0;
}
static int cb_thread(SceSize args, void *argp) {
    int id = sceKernelCreateCallback("exit_cb", exit_cb, NULL);
    sceKernelRegisterExitCallback(id);
    sceKernelSleepThreadCB();
    return 0;
}
static void setup_callbacks(void) {
    int t = sceKernelCreateThread("cb_thread", cb_thread, 0x11, 0xFA0, 0, NULL);
    if (t >= 0) sceKernelStartThread(t, 0, NULL);
}

/* ---------- Drawing ---------- */
static void rect(int x, int y, int w, int h, unsigned int c) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > W) w = W - x;
    if (y + h > H) h = H - y;
    if (w <= 0 || h <= 0) return;
    for (int yy = y; yy < y + h; yy++) {
        unsigned int *p = draw_fb + yy * STRIDE + x;
        for (int xx = 0; xx < w; xx++) p[xx] = c;
    }
}

static void begin_frame(void) {
    pspDebugScreenSetOffset(draw_off);
}

static void end_frame(void) {
    sceDisplayWaitVblankStart();
    sceDisplaySetFrameBuf((void *)((unsigned int)sceGeEdramGetAddr() + draw_off),
                          STRIDE, PSP_DISPLAY_PIXEL_FORMAT_8888,
                          PSP_DISPLAY_SETBUF_NEXTFRAME);
    draw_off ^= FRAME_BYTES;                 /* flip buffers */
    draw_fb = (unsigned int *)(0x44000000 + draw_off);
}

#define TEXT(x, y, ...) do { pspDebugScreenSetXY(x, y); pspDebugScreenPrintf(__VA_ARGS__); } while (0)

/* ---------- Game ---------- */
int main(void) {
    setup_callbacks();
    pspDebugScreenInit();
    pspDebugScreenEnableBackColor(0);
    pspDebugScreenSetTextColor(0xFFFFFFFF);
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);
    srand(12345);

    draw_off = FRAME_BYTES;                  /* first frame drawn into buffer 1 */
    draw_fb  = (unsigned int *)(0x44000000 + draw_off);

    int px = 230, py = 132, room = 0, encounters = 0;
    int hp = 20, maxhp = 20, coins = 0;
    int mode = DIALOGUE, dialog = 0;
    int enemy_hp = 12, soul_x = 240, soul_y = 205, invuln = 0, turn = 0;
    int last_ex = -999, last_ey = -999;
    unsigned int prev = 0;
    SceCtrlData pad;

    while (1) {
        sceCtrlPeekBufferPositive(&pad, 1);
        unsigned int b = pad.Buttons;
        unsigned int pressed = b & ~prev;    /* edge-triggered buttons */
        prev = b;
        int act = pressed & (PSP_CTRL_CROSS | PSP_CTRL_CIRCLE);

        switch (mode) {
        case EXPLORE: {
            int sp = (b & PSP_CTRL_RTRIGGER) ? 3 : 2;
            if (b & PSP_CTRL_LEFT)  px -= sp;
            if (b & PSP_CTRL_RIGHT) px += sp;
            if (b & PSP_CTRL_UP)    py -= sp;
            if (b & PSP_CTRL_DOWN)  py += sp;
            if (px < 22) px = 22;
            if (px > 458) px = 458;
            if (py < 36) py = 36;
            if (py > 232) py = 232;

            if (pressed & PSP_CTRL_START) { dialog = 0; mode = DIALOGUE; }
            else if (px > 438 && py > 105 && py < 170) {
                room = (room + 1) % 3;
                px = 35; py = 135; dialog = 1; mode = DIALOGUE;
            } else if ((pressed & PSP_CTRL_CROSS) &&
                       px > 190 && px < 285 && py > 90 && py < 165) {
                dialog = 2; mode = DIALOGUE;
            } else if (encounters < 8 &&
                       abs(px - last_ex) + abs(py - last_ey) > 100 &&
                       rand() % 700 == 0) {
                mode = BATTLE;
                enemy_hp = 12 + rand() % 5;
                soul_x = 240; soul_y = 205; turn = 0; invuln = 0;
                encounters++; last_ex = px; last_ey = py;
            }
            break;
        }
        case DIALOGUE:
            if (act || (pressed & PSP_CTRL_START)) mode = EXPLORE;
            break;
        case BATTLE: {
            if (b & PSP_CTRL_LEFT)  soul_x -= 3;
            if (b & PSP_CTRL_RIGHT) soul_x += 3;
            if (b & PSP_CTRL_UP)    soul_y -= 3;
            if (b & PSP_CTRL_DOWN)  soul_y += 3;
            if (soul_x < 154) soul_x = 154;
            if (soul_x > 326) soul_x = 326;
            if (soul_y < 155) soul_y = 155;
            if (soul_y > 245) soul_y = 245;

            if (invuln > 0) invuln--;
            int bx = 170 + (turn * 17) % 140;
            int by = 160 + (turn * 11) % 75;
            if (invuln == 0 && abs(soul_x - bx) < 9 && abs(soul_y - by) < 9) {
                hp -= 2; invuln = 35;
                if (hp <= 0) { hp = 0; mode = GAMEOVER; }
            }
            turn++;
            if (pressed & PSP_CTRL_CROSS) {
                enemy_hp -= 4;
                if (enemy_hp <= 0) { mode = VICTORY; coins += 5; }
            }
            if (pressed & PSP_CTRL_CIRCLE) {
                enemy_hp -= 2;
                if (enemy_hp <= 0) { mode = VICTORY; coins += 3; }
            }
            break;
        }
        case VICTORY:
            if (act) { mode = EXPLORE; px = 240; py = 132; }
            break;
        case GAMEOVER:
            if (pressed & PSP_CTRL_CROSS) { hp = maxhp; mode = EXPLORE; px = 240; py = 132; }
            break;
        }

        /* ---------- render ---------- */
        begin_frame();
        rect(0, 0, W, H, 0xFF2A1515);        /* ABGR: dark navy */

        if (mode == EXPLORE || mode == DIALOGUE) {
            unsigned int floor_c = room == 0 ? 0xFF552E30 : room == 1 ? 0xFF4C4625 : 0xFF2E4056;
            rect(14, 26, 452, 226, 0xFF56314A);
            rect(22, 34, 436, 210, floor_c);
            for (int y = 44; y < 238; y += 24)
                for (int x = 30; x < 450; x += 28)
                    rect(x, y, 2, 2, 0xFF7A616B);
            rect(438, 105, 20, 65, 0xFF6DC6FF);              /* door */
            rect(224, 116, 18, 25, 0xFFB2E06B);              /* NPC */
            rect(228, 108, 10, 10, 0xFFA0D6FF);
            rect(px - 6, py - 7, 12, 15, 0xFF4AD2FF);        /* player */
            rect(px - 3, py - 10, 6, 5, 0xFFB5F0FF);

            TEXT(1, 0, "SUNSET SOUL | Room %d/3  HP %d/%d  Stars %d", room + 1, hp, maxhp, coins);
            TEXT(1, 1, "D-pad move  R run  X talk  START help");
            if (mode == DIALOGUE) {
                rect(10, 190, 460, 76, 0xFF000000);
                rect(12, 192, 456, 72, 0xFF3A2020);
                if (dialog == 1)
                    TEXT(3, 25, "You step into a new place.\n   The air smells like rain.\n\n   X / O: continue");
                else if (dialog == 2)
                    TEXT(3, 25, "TRAIL GUIDE: The dark isn't always\n   dangerous. Take a breath, little star.\n   X / O: continue");
                else
                    TEXT(3, 25, "SUNSET SOUL\n   An original tiny RPG for PSP.\n   X / O: continue");
            }
        } else if (mode == BATTLE) {
            rect(0, 0, W, H, 0xFF1B0F10);
            rect(120, 35, 240, 70, 0xFF56314A);
            rect(132, 47, 216, 46, 0xFF221717);
            rect(220, 53, 40, 30, 0xFFD8A0B7);               /* enemy */
            rect(213, 62, 54, 17, 0xFFD8A0B7);
            rect(228, 62, 5, 5, 0xFF392025);
            rect(247, 62, 5, 5, 0xFF392025);
            rect(138, 132, 204, 122, 0xFFFFFFFF);            /* box */
            rect(142, 136, 196, 114, 0xFF201414);
            rect(170 + (turn * 17) % 140 - 4, 160 + (turn * 11) % 75 - 4, 8, 8, 0xFF5F80FF);
            if (invuln == 0 || invuln % 4 < 2)
                rect(soul_x - 5, soul_y - 5, 10, 10, 0xFF784CFF);
            TEXT(1, 0, "A WANDERING SHADE  HP %d/%d  ENEMY %d", hp, maxhp, enemy_hp);
            TEXT(1, 1, "D-pad dodge  X strike  O spare");
        } else if (mode == VICTORY) {
            rect(0, 0, W, H, 0xFF3B1D25);
            rect(185, 65, 110, 100, 0xFF4AD2FF);
            rect(205, 85, 70, 60, 0xFFB5F0FF);
            TEXT(3, 20, "YOU FOUND A LITTLE LIGHT!  X / O: back");
        } else {
            rect(0, 0, W, H, 0xFF1A0D12);
            TEXT(4, 9, "THE LIGHT WENT QUIET");
            TEXT(4, 11, "Your journey isn't over. Press X.");
        }
        end_frame();
    }
    return 0;
}
