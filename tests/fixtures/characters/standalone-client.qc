// SPDX-License-Identifier: GPL-3.0-or-later
#define CSQC
#include "csplat.qc"

entity actors[3];
float skeletons[3], clips[3], hand[3];
vector previousHand[3];
float ready, started, lastSample, stage, failed;

void() CSQC_Input_Frame = {};

void(float api, string engine, float version) CSQC_Init = {
    started = time;
    actors[0] = spawn();
    actors[1] = spawn();
    actors[2] = spawn();
    precache_model("models/casey.iqm");
    precache_model("models/drew.iqm");
    precache_model("models/jamie.iqm");
    setmodel(actors[0], "models/casey.iqm");
    setmodel(actors[1], "models/drew.iqm");
    setmodel(actors[2], "models/jamie.iqm");
    setorigin(actors[0], '0 55 0');
    setorigin(actors[1], '0 0 0');
    setorigin(actors[2], '0 -55 0');
};

void() CSQC_Shutdown = {
    for (int i = 0; i < 3; i++) {
        if (skeletons[i]) {
            actors[i].skeletonindex = 0;
            skel_delete(skeletons[i]);
            skeletons[i] = 0;
            print("RB21_SKELETON_RELEASED ", ftos(i), "\n");
        }
    }
};

float() checkRigs = {
    for (int i = 0; i < 3; i++) {
        float model = actors[i].modelindex;
        clips[i] = frameforname(model, "walk_forward");
        if (clips[i] < 0)
            return FALSE;
        if (!skeletons[i])
            skeletons[i] = skel_create(model);
        if (!skeletons[i])
            return FALSE;
    }
    float reference = skeletons[0];
    float count = skel_get_numbones(reference);
    if (count != 79) {
        print("RB21_BAD_BONE_COUNT ", ftos(count), "\n");
        failed = TRUE;
        return FALSE;
    }
    for (int i = 0; i < 3; i++) {
        float rig = skeletons[i];
        if (skel_get_numbones(rig) != count) {
            failed = TRUE;
            return FALSE;
        }
        for (float bone = 1; bone <= count; bone++) {
            if (skel_get_bonename(reference, bone) != skel_get_bonename(rig, bone) ||
                skel_get_boneparent(reference, bone) != skel_get_boneparent(rig, bone)) {
                print("RB21_RIG_MISMATCH ", ftos(i), " ", ftos(bone), "\n");
                failed = TRUE;
                return FALSE;
            }
            if (skel_get_bonename(rig, bone) == "wrist.R")
                hand[i] = bone;
        }
        if (!hand[i]) {
            failed = TRUE;
            return FALSE;
        }
    }
    print("RB21_RIGS_READY 3 79 walk_forward\n");
    return TRUE;
};

void(float width, float height, float notmenu) CSQC_UpdateView = {
    if (!ready && !failed) {
        ready = checkRigs();
        if (!ready && time - started > 10) {
            print("RB21_MODEL_LOAD_TIMEOUT\n");
            failed = TRUE;
        }
    }

    clearscene();
    setproperty(VF_VIEWPORT, '0 0 0', [width, height, 0]);
    setproperty(VF_ORIGIN, '-154 0 68');
    setproperty(VF_ANGLES, '12 0 0');
    setproperty(VF_FOVX, 76);
    setproperty(VF_FOVY, 52);
    setproperty(VF_DRAWWORLD, TRUE);
    if (ready) {
        float phase = time - started;
        for (int i = 0; i < 3; i++) {
            entity actor = actors[i];
            actor.frame = clips[i];
            actor.frame2 = clips[i];
            actor.frame1time = phase;
            actor.lerpfrac = 0;
            // Each model retains its own bind transforms and plays the same
            // named animation at the same clock, even at different proportions.
            actor.skeletonindex = skel_build(skeletons[i], actor, actor.modelindex, 0, 0, 0);
            addentity(actor);
        }
        if (stage < 4 && time >= lastSample + .19) {
            lastSample = time;
            stage++;
            for (int i = 0; i < 3; i++) {
                vector current = skel_get_boneabs(skeletons[i], hand[i]);
                print("RB21_WRIST ", ftos(stage), " ", ftos(i), " ", ftos(vlen(current - previousHand[i])), "\n");
                previousHand[i] = current;
            }
        }
    }
    renderscene();
};
