#ifndef GAME_ENEMYBASE_H
#define GAME_ENEMYBASE_H

// The base class of the enemies (and a few enemy-like actors): 52 game
// classes in about 50 units derive from it (37 directly), e.g. act_rope,
// act_gibdo, act_beamos, act_wallmaster, act_vulture, act_floormaster, the
// Shadow Link family (sub-base 0x804A6A00), act_vaati_boss; also act_navi,
// act_kaepora_gaebora, act_horse_race_man and act_tingle_buster.
// Its code is game/enemy_base_801CDC8C.cpp; its vtable is at 0x804A4808
// (__vt__18EnemyBase_804A4808). "EnemyBase" is provisional (docs/PLAN.md,
// "Placeholder names"): its update() runs the hit handling of ActorBase
// (updateHitReaction_801F3D94, checkPlayerAttacks_801F2FAC, checkActorHit_801F2DB4, damagePlayersInBox_801F2B7C) and a death hook once
// mLife reaches 0, and most descendants are enemies in docs/names/actors.tsv.
//
// Destructor: inline. The vtable's slot 0x08 is __dt__18EnemyBase_804A4808Fv
// (0x801B7E1C, dtk's dtor_801B7E1C before G1), which lies
// in act_rope (outside enemy_base_801CDC8C): the weak copy of an inline destructor,
// kept from the first object that emitted it. It stores this vtable,
// destroys mAnim (its J3DFrameCtrl: __vt__12J3DFrameCtrl at 0x294) and calls
// ActorBase's destructor; derived destructors inline the same code.
//
// Layout: from the constructor (0x801D0F40), init (0x801D0ED0) and update
// (0x801CDD58). sizeof == 0x2A8: mAnim (0x38 bytes) ends there; derived
// classes put their own fields at 0x2A8 and up (act_floormaster lwz 0x2B8,
// tu_801DE714 0x2A8, act_thief 0x338).
//
// Owner: the shared-header owner (docs/COORDINATOR.md). Game-file agents
// propose changes as SHARED-EDITS.

#include "types.h"
#include "game/ActorBase.h"
#include "game/SpriteAnim.h"

// Bits of EnemyBase_804A4808::mFlags (0x230), from update() (0x801CDD58)
// and the movement steps (0x801CE120, 0x801CE694). update() clears the low
// five bits each frame (clrrwi 5, 0x801CDDA8).
enum EnemyFlag_804A4808 {
    ENEMY_FLAG_HIT = 0x1,          // updateHitReaction_801F3D94 reported a hit this frame
    ENEMY_FLAG_HIT_PLAYER = 0x2,   // checkHitByPlayer_0x5C held (then onHitByPlayer_0x6C)
    ENEMY_FLAG_HIT_ACTOR = 0x4,    // checkHitByActor_0x60 held (then onHitByActor_0x70)
    ENEMY_FLAG_LANDED = 0x8,       // set by fn_801CE694 / fn_801CE800
    ENEMY_FLAG_NO_FLOOR = 0x20,    // fn_801CE694 skips the floor check
    ENEMY_FLAG_BOUNCE = 0x40,      // move_0x98 bounces off the ground (onBounce_0x7C)
    ENEMY_FLAG_NO_HURT = 0x80,     // updateHitReaction_801F3D94 gets false
    ENEMY_FLAG_NO_ATTACK = 0x100,  // skips attackPlayers_0x68
    ENEMY_FLAG_KEEP_LIFE = 0x200,  // mLife = mLifeBackup before and after the hits
    ENEMY_FLAG_STARTED = 0x400,    // onFirstUpdate_0x54 ran
    ENEMY_FLAG_DEAD = 0x800,       // onDeath_0x78 ran (mLife <= 0)
    ENEMY_FLAG_NO_MAP_HIT = 0x1000, // sweepMap_0x64 skips the map sweep
};

class EnemyBase_804A4808 : public ActorBase_804A5770 {
public:
    // 0x801D0F40; 37 units call it (the spawner's inline constructors and
    // the sub-bases' constructors), right after ActorBase's.
    EnemyBase_804A4808();

    // Virtual table at 0x804A4808: 39 slots, ActorBase's 19 and 20 new ones
    // (0x54-0xA0). Overrides of the old slots: the destructor (all 52),
    // init (49), update (14), vfunc_0x10 (3), draw (39), die_0x24 (9),
    // vfunc_0x44 (2), isBlocked_0x4C (7).
    //
    // The new slots were checked against all 52 descendant vtables (the
    // sub-bases 0x804A5B28 and 0x804A3D7C append 2 and 4 more) and every
    // call through them: the calls are all in this unit (update() and the
    // movement steps), except qualified (non-virtual) calls from overrides,
    // e.g. fn_801DC790. "N overrides" counts descendants whose slot differs;
    // "reads" is what the overrides read (r4-r10, f1-f8) before writing.
    // Names are offsets until the role is proven; the comment says what the
    // caller does around the call.
    virtual ~EnemyBase_804A4808() {}
    // 0x801D0ED0. Sets mBox and mMapBox_0x80 to (-8, -8)-(8, 8), field_0xb0 =
    // 0x11101, mDir_0x98 = randInt_801CD664(4) (a random direction) and
    // mLifeBackup = mLife; 40 derived init()s call it first.
    /* 0x0C */ virtual void init();
    // 0x801CDD58, the per-frame driver (tu_801D8400 and act_iron_pot_801E6D48 call it
    // from their own update()): onFirstUpdate_0x54 once, the hit checks
    // (vfunc_0x58, updateHitReaction_801F3D94, checkHitByPlayer_0x5C/0x6C or checkHitByActor_0x60/0x70),
    // onHitReactionEnd_0x74 when mHitTimer_0xd8 drops from 1 to 0, onDeath_0x78 once mLife
    // <= 0, then move_0x98, sweepMap_0x64, attackPlayers_0x68 into mPlayersHit,
    // updateBehavior_0x9C(sweepMap_0x64's result), and vfunc_0xA0 unless
    // isUpperLayer_801CD2F8(mLayer_0x4).
    /* 0x14 */ virtual void update();

    // 0x801D0ECC, empty. 9 overrides, no arguments read; update() calls it
    // once, when it sets ENEMY_FLAG_STARTED (0x801CDD90).
    /* 0x54 */ virtual void onFirstUpdate_0x54();
    // 0x801CECEC, returns 0. 9 overrides, no arguments read, all return 0
    // or 1 (li/mr r3 of a 0/1 value); update() ignores r3 (0x801CDE04).
    /* 0x58 */ virtual bool vfunc_0x58();
    // 0x801CEB78: checkPlayerAttacks_801F2FAC() (hit by a player) when mBox is not empty,
    // else 0. 16 overrides, no arguments read; update() tests r3 with
    // clrlwi. and then sets ENEMY_FLAG_HIT_PLAYER and calls onHitByPlayer_0x6C.
    /* 0x5C */ virtual u8 checkHitByPlayer_0x5C();
    // 0x801CE9F8: checkActorHit_801F2DB4('NULL', 'NULL', NULL) (hit by an actor) when
    // mBox is not empty, else 0. 19 overrides, no arguments read; update()
    // tests r3 with clrlwi. and then sets ENEMY_FLAG_HIT_ACTOR and calls
    // onHitByActor_0x70.
    /* 0x60 */ virtual u8 checkHitByActor_0x60();
    // 0x801CE98C: unless ENEMY_FLAG_NO_MAP_HIT, sweepMapBox_801F2618(&mMapHitDir,
    // false) when mMapBox_0x80 is not empty, else 0. 5 overrides, no
    // arguments read; update() passes r3 on to updateBehavior_0x9C (clrlwi 24).
    // checkHitByPlayer_0x5C/0x60/0x64 are u8 like the ActorBase hit tests they return
    // (checkPlayerAttacks_801F2FAC / checkActorHit_801F2DB4 / sweepMapBox_801F2618 results go out with mr, no
    // bool conversion; overrides likewise).
    /* 0x64 */ virtual u8 sweepMap_0x64();
    // 0x801CE928: damagePlayersInBox_801F2B7C(mParamAtcp_0x114, -1, 1) (the players hit, a
    // bitmask) when mBox is not empty, else 0. 9 overrides, no arguments
    // read; update() stores r3 with stb to mPlayersHit (0x801CDFB0).
    /* 0x68 */ virtual u8 attackPlayers_0x68();
    // 0x801CE924, empty. 4 functions in 7 overrides, no arguments read;
    // update() calls it after checkHitByPlayer_0x5C held.
    /* 0x6C */ virtual void onHitByPlayer_0x6C();
    // 0x801CE920, empty, never overridden; update() calls it after
    // checkHitByActor_0x60 held.
    /* 0x70 */ virtual void onHitByActor_0x70();
    // 0x801CE91C, empty. 8 functions in 13 overrides, no arguments read;
    // update() calls it when mHitTimer_0xd8 was 1 before the hit checks and is
    // 0 after (0x801CDF0C).
    /* 0x74 */ virtual void onHitReactionEnd_0x74();
    // 0x801CE918, empty. 1 override (act_wallmaster), no arguments read;
    // update() calls it once, when mLife <= 0 and it sets ENEMY_FLAG_DEAD.
    /* 0x78 */ virtual void onDeath_0x78();
    // 0x801CE914, empty. 1 override (act_terrorpin), no arguments read;
    // move_0x98 / fn_801CE32C call it when the actor bounces off the
    // ground (ENEMY_FLAG_BOUNCE, |velocity z| still >= 0.1f).
    /* 0x7C */ virtual void onBounce_0x7C();
    // 0x801CE694, the floor check after a step with mPos.z <= 0 on a layer
    // whose GameView byte 0x131DF + layer is set. 2 overrides
    // (tu_801DE714, tu_801D8400; both also call it), no arguments read.
    /* 0x80 */ virtual void checkFloor_0x80();
    // 0x801CE594, gravity: unless mHitTimer_0xd8 > 0 && mKnockbackKind_0xf8 == 2,
    // mVel_0x3c.z += -0.2f, clamped at -4.0f. 1 override
    // (act_shadow_link_2), no arguments read; calcVelocity_0x88 and fn_801CE5D4
    // call it.
    /* 0x84 */ virtual void applyGravity_0x84();
    // 0x801CE4E8: mVel_0x3c.xy = mSpeed along mDir_0x98 (direction 0-3),
    // then applyGravity_0x84. 25 overrides, no arguments read; move_0x98 and
    // fn_801CE32C call it before moving; 6 units call the base.
    /* 0x88 */ virtual void calcVelocity_0x88();
    // 0x801CE4E4, empty. 2 overrides (tu_801DE714, act_thief), no arguments
    // read; called right after mPos += mVel_0x3c.
    /* 0x8C */ virtual void onMoved_0x8C();
    // 0x801CE4E0, empty. 36 overrides, the per-state behaviour: 7 of them
    // test r4 with clrlwi. 24 first (act_thief 0x802055BC, act_gibdo
    // 0x801B9AC4, act_shadow_link_2 0x801C2EE0); updateBehavior_0x9C and fn_801CE2BC
    // call it with their own r4 still in place.
    /* 0x90 */ virtual void updateState_0x90(u8 mapHit);
    // 0x801CE4BC: mAnim.update_801D1C18() (advance the sprite animation). 13
    // overrides, no arguments read; 11 units call the base.
    /* 0x94 */ virtual void updateAnim_0x94();
    // 0x801CE120, the movement step, unless one of the hit bits is set:
    // mSpeed toward mTargetSpeed (moveToward_801CD950), calcVelocity_0x88, mPos +=
    // mVel_0x3c, onMoved_0x8C, then the ground: checkFloor_0x80 on floor layers,
    // a bounce (ENEMY_FLAG_BOUNCE: velocity * -0.6f, onBounce_0x7C) or a stop
    // at z = 0. 7 overrides, no arguments read; fn_801CE32C is the same
    // without the hit-bit test.
    /* 0x98 */ virtual void move_0x98();
    // 0x801CE0A4: unless one of the hit bits is set, mStateTimer++ (kept
    // >= 0), updateState_0x90(mapHit), updateAnim_0x94. 17 functions in 22 overrides,
    // 4 of them read r4 (act_horse_race_man, act_vaati_boss, act_thief,
    // act_kaepora_gaebora); update() passes sweepMap_0x64's result with
    // clrlwi 24 (0x801CDFB8), as for an unsigned char parameter (a bool
    // local passed to a bool parameter is a plain mr; see updateHitReaction_801F3D94 in
    // ActorBase.h). fn_801CE2BC is the same without the test.
    /* 0x9C */ virtual void updateBehavior_0x9C(u8 mapHit);
    // 0x801CE00C, empty. 14 functions in 20 overrides, no arguments read;
    // update() calls it last, when isUpperLayer_801CD2F8(mLayer_0x4) is false.
    /* 0xA0 */ virtual void vfunc_0xA0();

    // Non-virtual members other units call: the exports of
    // game/enemy_base_801CDC8C.cpp. Every call passes an enemy in r3 (the caller's
    // own `this` in all but tu_802144F0, which passes the enemy it keeps at
    // 0x34). Signatures come from the register use in each body and at the
    // call sites; "N units / M calls" counts the other units and all calls.
    // As in ActorBase.h, the names stay dtk's placeholders and symbols.txt
    // carries the mangled name each declaration produces
    // (fn_XXXXXXXX__18EnemyBase_804A4808F...), so a parameter type changed
    // here is a symbols.txt rename too; the return type is not mangled.
    // Where a function takes ints and floats, ints first is a guess.
    // "Direction" is 0-3 as in mDir_0x98 (fn_801CE4E8: 0 +x, 1 -x, 2 +y,
    // 3 -y); "player" is a player index 0-3 into mPlayerPos_0x1ac.

    // 2 units / 6 calls. Stops sound `soundId` on the actor's sound object
    // (fn_801F1184, JAUSoundObject::stopSound(JAISoundID(soundId), 0)); the
    // id is stored with stw for the JAISoundID.
    void fn_801CDC8C(u32 soundId);
    // 2 units / 2 calls. Starts sound `soundId` (startSound_801F0E34(soundId, 0))
    // with mLayer_0x4 and mPos set to layer and *pos for the call, then
    // restores them.
    void fn_801CDCD0(s32 layer, const JGeometry::TVec3f* pos, u32 soundId);
    // 1 unit / 2 calls (tu_8021827C, this enemy's vfunc_0xA0). The screen
    // position (x, y - z, 0) to *out, its y raised to at least the view
    // box's top (0xCC or 0xDC by layer) + 1.0f (fsel).
    void fn_801CE010(JGeometry::TVec3f* out);
    // 8 units / 8 calls (overrides of updateBehavior_0x9C). updateBehavior_0x9C's body
    // without the hit-bit test; r4 reaches updateState_0x90 unchanged.
    void fn_801CE2BC(u8 mapHit);
    // 5 units / 5 calls (overrides of move_0x98). move_0x98's body
    // without the hit-bit test.
    void fn_801CE32C();
    // 2 units / 2 calls. If a hit bit is set: knock-back step (mPos.z =
    // mPrevPos.z, mVel_0x3c.z = mPrevVel.z, applyGravity_0x84, then the ground
    // as in move_0x98).
    void fn_801CE5D4();
    // 1 unit / 1 call (act_shadow_link_2_801C9AF8). The landing half of checkFloor_0x80 (map
    // tile under mMapBox_0x80's box via getTileCollisionCodeAlt_802265FC) without its own tests;
    // sets ENEMY_FLAG_LANDED.
    void fn_801CE800();
    // 2 units / 2 calls (act_vulture, act_wallmaster vfunc_0x60s). Hit by
    // the overlapping actor (findOverlappingActor_801F82CC) unless it is a live 'BMTY'
    // (fn_801CC8E4): takeHit_801F37AC with its attack and direction; sets its
    // field_0x164 when it landed. Returns 0 or 1.
    bool fn_801CEA60();
    // 3 units / 3 calls. For each player overlapping this actor and
    // passing startTimer_801EAC20 (field_0x1dc): fn_801CB538 at the hit point; then
    // sound 0x400D1. Returns whether any player was hit (u8: Beamos
    // checkHitByPlayer_0x5C keeps r3 raw, mr).
    u8 fn_801CEBD0();
    // 3 units / 3 calls (callers pass a 4CC in r4 and two lfs constants,
    // and test r3 with clrlwi.). Spawns `actorId` (spawnActor_801F9484) aimed at
    // the nearest player within `range` (f2, squared at 0x801CED3C) with
    // speed `speed` (f1 scales the direction, 0x801CF0A0); returns whether
    // one was spawned.
    bool fn_801CECF4(u32 actorId, f32 speed, f32 range);
    // 1 unit / 1 call (tu_801D2C60, stored to mDir_0x98). The direction
    // of mVel_0x3c (atan2 to a 16-bit angle, clrlwi 16, then angleToDir_801F681C).
    s32 fn_801CF0F4() const;
    // 1 unit / 4 calls (clrlwi.). fn_801CCB1C on (x, y - z - 16, 0) and
    // mLayer_0x4: a map test 16 units in front; returns 0 or 1.
    bool fn_801CF144() const;
    // 1 unit / 3 calls (act_gibdo: r4 = li 1 in the first, a 0/1 bool
    // local in the third; two lfs constants). A
    // direction toward the nearest player within `range` (f1, squared),
    // turning only when off by more than `margin` (f2); `turn` (clrlwi.
    // at 0x801CF53C) allows random turns (fn_801CF6B8). Callers compare r3
    // with mDir_0x98.
    s32 fn_801CF1B0(bool turn, f32 range, f32 margin);
    // 3 units / 7 calls (stored to mDir_0x98). A random direction
    // (randInt_801CD664(4)), or pickNewDir_801CF720's when blocked (isStepBlocked_801CFCF0).
    s32 fn_801CF6B8();
    // 3 units / 9 calls (callers pass mDir_0x98 or li 3; stored to
    // mDir_0x98). A random direction other than `dir` (fn_801CD580),
    // or fn_801CF798's when that one is blocked.
    s32 pickNewDir_801CF720(s32 dir);
    // 1 unit / 3 calls (cmpw against both arguments). The first direction
    // that is neither `dir` nor `except` and not blocked.
    s32 fn_801CF798(s32 dir, s32 except);
    // 4 units / 15 calls (clrlwi.). Whether the way from `from` to `to`
    // (x, y) on `layer` is clear, tested with isPathBlocked_801CFA70 at the ends and
    // in between.
    bool isWayBlocked_801CF888(const JGeometry::TVec2f* from, const JGeometry::TVec3f* to, s32 layer);
    // 1 unit / 5 calls (clrlwi; cmplwi 1). Both points inside the view's
    // box for `layer` (0xCC, or 0xDC for layer 8 and up) and the map
    // between them clear (getTileCollisionCode_8022461C-style lookups on layer).
    // It reads only (x, y) of both points, and isWayBlocked_801CF888 passes it 8-byte
    // TVec2f slots: the original's parameters were likely TVec2f (as are
    // getTileCollisionCodeAlt_802265FC's and fn_801CCD34's: checkFloor_0x80 / fn_801CE800 pass them
    // 8-byte slots too). Kept TVec3f for the TVec3f callers (act_shadow_link_2_801C5B08).
    bool isPathBlocked_801CFA70(const JGeometry::TVec3f* from, const JGeometry::TVec3f* to, s32 layer);
    // 1 unit / 6 calls (clrlwi.). fn_801CFD68(dir, 6.0f + the box edge on
    // that side): whether a step in direction `dir` is free.
    bool isStepBlocked_801CFCF0(s32 dir);
    // 1 unit / 5 calls (act_rope: li 0-3 and an f1). Whether moving `dist`
    // in direction `dir` stays clear of the map; 0 or 1.
    bool fn_801CFD68(s32 dir, f32 dist);
    // 6 units / 7 calls (r4 = a position, r5 = a player, r6 = li 0;
    // callers compare r3 with mDir_0x98). The direction from *pos to the
    // player, on the dominant axis unless `axis` (clrlwi.) picks the other.
    s32 fn_801D0020(const JGeometry::TVec3f* pos, s32 player, bool axis);
    // 4 units / 4 calls. The velocity (x, y) toward `player` at `speed` to
    // *out (stfs 0 and 4 only; callers copy two floats to mVel_0x3c).
    // TVec2f is a guess (TVec3f is as likely).
    void fn_801D00B4(s32 player, JGeometry::TVec2f* out, f32 speed);
    // 9 units / 12 calls. The same toward *target (x, y).
    void calcVelocityToward_801D01A0(const JGeometry::TVec3f* target, JGeometry::TVec2f* out, f32 speed);
    // 8 units / 11 calls. The 16-bit angle (atan2 * 10430.378f, fctiwz) from
    // mPos to `player` (mPlayerPos_0x1ac[player]); callers store it with sth or
    // extsh it, the body returns it unnarrowed.
    s32 getAngleToPlayer_801D0280(s32 player) const;
    // 4 units / 8 calls. The same angle from mPos to *pos.
    s32 fn_801D02E0(const JGeometry::TVec3f* pos) const;
    // 1 unit / 3 calls. The player whose angle from *from is nearest to
    // `angle` (extsh at 0x801D035C), skipping players that are absent or
    // excluded, or -1.
    s32 fn_801D0338(const JGeometry::TVec3f* from, s16 angle);
    // 22 units / 49 calls. The nearest present player within `range`
    // (squared, f1) of *pos, or -1.
    s32 findNearestPlayer_801D05F8(const JGeometry::TVec3f* pos, f32 range);
    // 5 units / 8 calls (clrlwi.). Whether `player` can be targeted (mr.
    // r27, r4: >= 0 and present) and is within `range` of *pos.
    bool fn_801D08B4(s32 player, const JGeometry::TVec3f* pos, f32 range);
    // 9 units / 18 calls (clrlwi.). Whether `player` can be targeted (the
    // player tests fn_8022CAF8 / hasPlayerLeader_8022F514 and the actor-id list of
    // fn_801D0338).
    bool canTargetPlayer_801D0B44(s32 player);
    // 2 units / 6 calls (clrlwi.). Whether mPlayerPos_0x1ac[player] is within
    // `dist` of *pos (squared distance in x, y).
    bool fn_801D0D50(s32 player, const JGeometry::TVec3f* pos, f32 dist);
    // 5 units / 7 calls. The squared (x, y) distance from *pos to
    // mPlayerPos_0x1ac[player].
    f32 getPlayerDistSq_801D0DA0(s32 player, const JGeometry::TVec3f* pos);
    // 6 units / 13 calls (clrlwi.). On a floor layer: ENEMY_FLAG_LANDED or
    // 0x10 set; elsewhere mPos.z <= 0.
    bool isOnGround_801D0DD4() const;
    // 3 units / 3 calls. Snaps mPos (x, y) with fn_801CCD34 and copies it
    // to mPrevPos and mSpawnPos.
    void fn_801D0E58();

    // The step bodies move_0x98 / updateBehavior_0x9C share with fn_801CE32C /
    // fn_801CE2BC: the code is repeated in both (each is defined before the
    // other's use only once), so they are inline helpers of
    // game/enemy_base_801CDC8C.cpp.
    void moveStep();
    void stateStep(u8 mapHit);
    // The player search canTargetPlayer_801D0B44 / findNearestPlayer_801D05F8 / getPlayerDistSq_801D0DA0 do, inlined
    // in the other searches of the unit.
    bool isPlayerOnLayer(s32 player);
    bool canTargetPlayer(s32 player);
    f32 playerDistSq(s32 player, const JGeometry::TVec3f* pos);
    bool isPlayerNear(s32 player, const JGeometry::TVec3f* pos, f32 dist);
    s32 nearestPlayer(const JGeometry::TVec3f* pos, f32 range);
    s32 directionTo(const JGeometry::TVec3f* pos, s32 player, bool axis);
    void velocityToward(const JGeometry::TVec3f* target, JGeometry::TVec2f* out, f32 speed);

    // A flag test the original has as an inline: VaatiBoss onHitReactionEnd_0x74
    // (0x801D4AA8) loads mFlags for the test and again for the clear.
    bool checkFlag(u32 flag) const { return mFlags & flag; }
    // The hit-bits test update() (0x801CDEAC) has as a materialised bool
    // (clrlwi 29; neg; or; srwi.) before the ENEMY_FLAG_NO_ATTACK test.
    bool isHit() const { return (mFlags & 7) ? true : false; }
    // ShadowLink2's fn_801C9CB0 (0x801C9D4C) has this test as a materialised bool (mfcr; extrwi.).
    bool isOnGround() const { return mPos.z <= 0.0f; }

    /* 0x230 */ u32 mFlags; // EnemyFlag_804A4808
    /* 0x234 */ JGeometry::TVec3f mPrevVel; // mVel_0x3c at the start of update()
    /* 0x240 */ u8 mPlayersHit;  // attackPlayers_0x68's bitmask, cleared each update()
    /* 0x244 */ s32 mLifeBackup; // 8; init() copies mLife here
    /* 0x248 */ s32 mState;      // derived classes' state (act_gibdo cmpwi 3)
    /* 0x24C */ s32 mStateTimer; // updateBehavior_0x9C counts it up
    /* 0x250 */ s32 field_0x250;
    /* 0x254 */ f32 mSpeed;       // calcVelocity_0x88 moves along mDir_0x98 at this
    /* 0x258 */ f32 mTargetSpeed; // move_0x98 moves mSpeed toward it
    /* 0x25C */ f32 mAccel;       // 0.1f; the step while mSpeed < mTargetSpeed
    /* 0x260 */ f32 mDecel;       // 0.2f; the step while mSpeed > mTargetSpeed
    /* 0x264 */ JGeometry::TVec3f mMapHitDir; // sweepMap_0x64's sweepMapBox_801F2618 out
    /* 0x270 */ SpriteAnim_801D267C mAnim;
}; // Size: 0x2A8

// A const bool defined (false) in game/enemy_base_801CDC8C.cpp, first in its
// .sdata2. sweepMap_0x64 passes a literal 0 as sweepMapBox_801F2618's flag (li r5, 0),
// while overrides in act_thief (0x80204FC0), act_shadow_link_2_801C9AF8 and act_thief_80206B30
// load this byte for it (lbz r5): they see only this declaration. Its
// name is unknown.
extern const bool lbl_8053EFC0;

// Helpers of game/tu_801CB270.cpp the enemy code uses.

// Whether `layer` is 8 or more (cmpwi 8): the layers drawShadow_801EF830 and the
// view's boxes (0xDC instead of 0xCC) treat as the upper ones. 0 or 1.
bool fn_801CD1D0(s32 player);
// Takes a layer, returns a bool (clrlwi 24); act_vaati_boss's update tests it before its first state.
bool fn_801CD228(s32 layer);
bool isUpperLayer_801CD2F8(s32 layer);
// Moves *value toward target by `step` (forwards r3, f1, f2 to
// fn_801CD9F0). EnemyBase_804A4808's movement step passes &mSpeed,
// mTargetSpeed and mAccel or mDecel. Returns nonzero while the value has
// not reached the target (cls_804A61C8::fn_80210A6C tests r3 with cmpwi).
s32 moveToward_801CD950(f32* value, f32 target, f32 step);
// game/tu_801CB270.cpp: finds the closest active player on layer `layer` to *pos, or -1.
extern "C" s32 fn_801CCE3C(const JGeometry::TVec3f* pos, s32 layer);
// Moves *value towards target by the fraction `factor` (at most 1) of the
// distance; true while it is further than `threshold` away.
bool approachValue_801CD8E4(f32* value, f32 target, f32 factor, f32 threshold);
// Returns the next sprite direction for `value` (mDir_0x98 is stored back
// from it, then startAnim_801D22DC plays it): 0 -> 3, 1 -> 2, 3 -> 1, else 0.
s32 fn_801CD30C(s32 value);

// game/tu_801CB270.cpp. The centre of the 16x16 tile holding *in (x, y)
// into *out (checkFloor_0x80's landing, fn_801D0E58).
void fn_801CCD34(const JGeometry::TVec3f* in, JGeometry::TVec3f* out);
void fn_801CCCC4(JGeometry::TVec3f* pos);
// game/tu_801CB270.cpp. The opposite direction: 0 <-> 1, 2 <-> 3 (else 0).
s32 oppositeDir_801CD354(s32 dir);
// game/tu_801CB270.cpp: the step moveToward_801CD950 wraps.
s32 fn_801CD9F0(f32* value, f32 target, f32 step);
// Convert a position between the two view-box spaces: out = (in.x, in.y)
// minus the view's 0xCC/0xD0 box corner plus its 0xDC/0xE0 one (fn_801CD150),
// or the other way round (fn_801CD0D0); z is copied. r3 in, r4 out.
void fn_801CD0D0(const JGeometry::TVec3f* in, JGeometry::TVec3f* out);
void fn_801CD150(const JGeometry::TVec3f* in, JGeometry::TVec3f* out);
// game/tu_801CB270.cpp. Spawns an effect on `layer` at *pos; the two s32
// arguments are the effect's height and depth - 1.
void fn_801CB688(s32 layer, const JGeometry::TVec3f* pos, s32 arg5, s32 arg6);
// game/tu_801CB270.cpp: the player (isPlayerActive_8023F264) whose fn_80236228 value is
// the lowest, or -1 (none, or the lowest shared). ActorBase's death
// (die_0x24) compares it with the killer's fn_8022F340.
s32 fn_801CCDA4();
// game/tu_801CB270.cpp (not written yet; C linkage keeps the dtk name): an
// effect at *pos on `layer` like fn_801CB688, for strong hits (takeHit_801F37AC
// with attack > 4 and type 1, or type 5 with life left).
extern "C" void fn_801CB5CC(s32 layer, const JGeometry::TVec3f* pos, s32 arg5, s32 arg6);
// game/tu_801CB270.cpp (0x801CBB98, 0x704 bytes).
// Free function (r3 = a layer box, r4 = a position, r5 = a 16-bit angle, f1 = a distance, r6 = *out,
// a 2D point written to): called by act_kaepora_gaebora's setState_80219AFC and by shadow_link_base_80220F54.
void fn_801CBB98(const JGeometry::TBox2f* box, const JGeometry::TVec3f* spawnPos, s32 angle, f32 dist,
                 JGeometry::TVec2f* out);

// game/tu_801CB270.cpp (0x801CCF34): fn_80236228 for the player `player` stands for (getPlayerLeader_8022F3DC's
// while hasPlayerLeader_8022F514 says it has one).
s32 fn_801CCF34(s32 player);

// game/tu_801CB270.cpp (0x801CC350; 1 caller unit, act_terrorpin updateState_0x90). Reflects the 2D
// velocity *vel off *normal (C_VECReflect, PSVECMag; z is preserved).
void fn_801CC350(const JGeometry::TVec3f* normal, JGeometry::TVec3f* vel);
// game/tu_801CB270.cpp (0x801CC9F8; 1 caller unit, act_shadow_link_2_801C5B08). Whether pos and the points 7 units
// around it all have tile code 0 on `layer` (with the view's layer flag).
bool fn_801CC9F8(const JGeometry::TVec3f* pos, s32 layer);

// game/tu_801CB270.cpp (shotgun pass; C++ linkage). How far box `a` sticks out of box `b`, as
// the move that puts it back (*out); true if it does.
bool fn_801CC29C(const JGeometry::TBox2f* a, const JGeometry::TBox2f* b, JGeometry::TVec2f* out);
// game/tu_801CB270.cpp: moves *cur toward *target by `factor` of the way, the step clamped between
// minStep and maxStep; true once the point is on the target.
bool fn_801CD754(JGeometry::TVec2f* cur, const JGeometry::TVec2f* target, f32 factor, f32 minStep, f32 maxStep);

extern "C" {

// game/tu_801CB270.cpp (not written yet; 0x801CCFE4, 0xEC bytes). Whether
// player `player` is present: isPlayerActive_8023F264 and none of a list of player
// state tests (isPlayerDead_80236B50, fn_8022DCD8, ...). Callers test r3 with clrlwi.
bool fn_801CCFE4(s32 player);

// game/tu_801CB270.cpp (owner unit, not written yet; 1 caller unit,
// act_thief_80206B30 vfunc_0x58). Each fetches the actor a player's getter
// (fn_8023B2B0 / fn_8023B22C / fn_8023B358) names, with r3 (the player index)
// forwarded untouched, and tests its kind at 0x1A0 (and that it is alive).
// 'ARRW' with field_0x108 > 0: *out = its mVel_0x3c, then markDead_801F3668 on
// it; `out` is zeroed first. Returns whether it was found.
bool fn_801CC820(s32 player, JGeometry::TVec3f* out);
// 'BOME': *outPlayer = the index (-1 first), its field_0x254 = 1 and, when
// `out` is given, *out = (pos.x, pos.y, 0) minus its mVel_0x3c * lbl_8053EF90.
// Returns whether it was found.
bool fn_801CC538(s32 player, const JGeometry::TVec2f* pos, JGeometry::TVec3f* out, s32* outPlayer);
// 'PCDM' with field_0x108 > 0: *outPlayer = the index (-1 first), *outVel =
// its mVel_0x3c (zeroed first), then its field_0x108 = 0. Returns whether
// it was found.
bool fn_801CC640(s32 player, s32* outPlayer, JGeometry::TVec3f* outVel);
// game/tu_801CB270.cpp (owner unit, not written yet; 0x801CC730, 0xF0 bytes;
// 1 caller unit, act_vaati_boss_801E3618 vfunc_0x58). Like fn_801CC640 with NULL
// outputs: *outPlayer = -1 and *outVel zeroed when given; returns whether it
// was found.
bool fn_801CC730(s32 player, s32* outPlayer, JGeometry::TVec3f* outVel);
}

#endif
