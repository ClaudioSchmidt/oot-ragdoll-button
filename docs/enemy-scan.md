# Enemy ragdoll scan (static, 2026-11-22)

Sources: `romfs/Pack/Actor/*.pack.zs` (65 rig packs unpacked to `work/scan/u/<Pack>/`), `Pack/AI.Global.Product.111.pack.zs`
(`work/scan/u/AI.Global.Product.111/AI/`, 749 AINB), `romfs/AS/*.root.asb.zs` (dumped to `work/scan/asb/*.txt`),
`romfs/AnimationEvent/AsNode/*.baev.zs` (`work/scan/baev/`), `romfs/System/CombinationDataTableData/*.bgyml` (loose).
Scripts: `work/scan/` (`unp.py`, `collect.py`, `tables.py`, `closure.py`, `gen.py`, ...; results `collect.json`, `rows.json`).
AINB is not parsed: only node/module/string names (so "AI entry" = which modules are reachable, not their wiring/conditions).
Things I inferred are marked "(guess)".

## Key facts first

1. **No ASB in the whole game defines the clips `SmashDamage`, `DownWait`, `DownRecover`** (scanned every `romfs/AS/*.asb`:
   only an unrelated candle object has `DownWait`). So the `Common.action.ExecuteRagdollReaction` / `smash` path
   (`ASStartCommand=SmashDamage`, `ASDownWaitCommand=DownWait`, `ASDownRecoverCommand=DownRecover`) has **no get-up clip for any
   enemy**. `Common.action.RecoverFromRagdollReaction.module` has an input `IsSetIdleEvenIfDownRecoverCommandInvalid` and the
   ExecuteRagdollReaction/smash modules have `IsIgnoreNotExistsError`-style flags, so the design tolerates a missing clip:
   (guess) ragdoll lies for `GroundDownTime`/`DownTime` frames, then control returns to the AI (Wait) with a pose snap, no stand-up animation.
   `RecoverFromRagdollReaction.module` is referenced by no other AINB (orphan).
2. Real stand-up/get-up clips exist only for: **Dekubaba** (`GetUp`, `Down_St/Lp/Ed`, own `Dekubaba.action.getup` + `interruptdowndamage`; not ragdoll based),
   **Poe** (`GetUp`, `PoeBig.action.getup`; floating, not ragdoll), **IronKnuckle** (`StandUp` clip + `Enemy_IronKnuckle.action.StandUp.module`, plays AS `StandUp`
   with anime-driven move; its hit reaction uses `ExecuteRagdollReactionProcess` directly), DarkLink (Player-style `Damage_Down_*_StandUp`, own Player rig), NPC `Npc_Hylia_MN` skeleton (`Down`, `GetUp`, `NpcCommon.utility.SimpleGetUp`).
   `EnemyCommon.action.getup.module` (verb `GetUp`) just plays `ASCommand=SleepEnd` = wake-up from sleeping, not a ragdoll get-up.
3. Normal sword hits on Lizalfos/Dinolfos/Wolfos/Stalfos do **not** map to InterruptSmash. `InterruptSmash` (hash 3322560101) occurs in damage tables only
   for Stalfos/Stalchild/Peahat (`Spike`, `EnergyWall` rows, Strong+Smash), DarkLink (all rows, Smash column), Gohma_Larva (nearly all). Lizalfos ragdolls via
   **AutoRagdollReaction** (hit -> SmallDamage clip with `OverrideReaction` AS event -> reaction list SmallDamage/FullDynamic), see below.
4. Moblin has damage table `NoReaction` (every cell `None`): it never reacts to hits, only ragdolls on death.

## Summary table

Legend: rig = Structure present; lists = `name(NumReactionFrames, c = StartWithCurrentPose)` (-1 = until stable); getup clip = get-up animation after a ragdoll knockdown
(no = nothing named DownRecover/GetUp/StandUp in that actor's ASB; the generic modules fall back to idle, guess); smash entry = whether the action root wires `InterruptSmash` -> `Bokoblin.action.interruptsmash` -> `Common.action.smash` + `Common.action.ExecuteRagdollReaction` (almost every actor does, via the shared root);
auto = `IsEnableAutoRagdollReaction` (resolved through `$parent`); GDT = `GroundDownTime` frames from `RagdollReactionParam`; death = DeathProcedureType, `PetrifiedStartFrame`, `PetrifiedTimeType` (gray stone look) from DeathParam table.

| actor (pack) | rig | reaction lists | get-up clip | AI entry to knockdown | auto | GDT | death / gray hint |
|---|---|---|---|---|---|---|---|
| **Enemy_Lizalfos** (+Enemy_MiddleBoss_Lizalfos) | yes | FullDynamic(-1), SmallDamage(20,c) | no (ASB: Appear,Die,Repelled,SmallDamage,Stun_*,Uproar..) | InterruptSmash -> Bokoblin.interruptsmash; InterruptSmallHit = Lizalfos.action.InterruptSmallHit (SmallDamage clip, no ragdoll node); InterruptRepelled (calls `OneShotRagdollReactionRequestDisableAutoReactionNext`); Cower (calls `OneShotRagdollReactionEnd`) | **yes** (Enemy_Lizalfos RRC, ignore key 2675241661) | 80 | Ragdoll, petrify 0 / Small, CustomDeathStartTime 18; IsDieRagdollReaction true |
| **Enemy_Moblin** | yes | FullDynamic(-1) | no (Stun_St/Lp/Ed only) | InterruptSmash wired, but table NoReaction so unreachable by hits | no | 30 | Ragdoll, petrify 0 |
| **Enemy_Stalfos** (+AlwaysRespawn, ForChild, Weak) | yes | Damage(-1), ScatterBody(-1,c), SmallDamage_Middle(30) | no (Stun_*, Fall, Land) | InterruptSmash (table: Spike/EnergyWall Strong+Smash); MediumHit; InterruptDown; Stalfos.InterruptSmallHit/Die/Guard | **yes** | - (no RagdollReactionParam resolved) | Ragdoll, petrify 0 / Small; IsDieRagdollReaction false (own InterruptDie -> ragdolldie reachable) |
| **Enemy_Dinolfos** | yes | FullDynamic(-1), FullDynamic(-1,c), SmallDamage(20,c) (+Lizalfos copies) | no (LargeDamage, Repelled, Stun_*) | like Lizalfos (shares Enemy_Lizalfos AI; Dinolfos.InterruptDie/Repelled; override: intensity None -> HugeHit) | **yes** (inherits Lizalfos ActorParam) | 80 (inherit) | Ragdoll (inherit), IsDieRagdollReaction true |
| **Enemy_IronKnuckle_White/Black/Silver, MiddleBoss_IronKnuckle_Nabooru** | yes | BreakArmor(-1), DeathDropWeapon(-1), SmallDamage_Middle(15) | **StandUp clip** + `Enemy_IronKnuckle.action.StandUp` | `Enemy_IronKnuckle.action.InterruptSmallHit` uses **ExecuteRagdollReactionProcess** directly (ASCommand SmallDamageBack/Front, DownTime, ExtendDownTime, IsForceDown, BreakArmor) | yes | Silver 10 | petrify 0, DeathProcedureType hash 3594736239 (not Ragdoll) |
| **Enemy_Wolfos / Wolfos_White** | yes | Wolfos_Die(-1) only | no | InterruptSmash wired but no `FullDynamic` list in pack (guess: smash/auto would find nothing); custom InterruptHugeHit/Repelled/Guard; death via Bokoblin.interruptdie->ragdolldie | no (RRC = EnemyCommon default; AS has OverrideReaction event) | 30 | Ragdoll, petrify 0 / Small, IsDieRagdollReaction true |
| **Enemy_ReDead (+AlwaysRespawn, ClimaxOnly), Enemy_Gibdo** | yes | FullDynamic(-1), SmallDamage(15) | no (ReDead: Stun_*, SitDown, Standing; Gibdo also Regain/WakeUp = coffin sleep) | InterruptSmash, InterruptDown, ReDead.interruptsmalldamage, Redead.interruptdie (-> ragdolldie + `QueryRagdollReactionIsStable`) | **yes** | 900 (30 s) | Ragdoll, petrify 0 |
| **Enemy_Stalchild (+Big)** | yes | CutNeck(-1), Die(-1), ScatterBody_Hose(-1,c), ScatterBody_NoNeck_Hose(-1,c) | no | SmallHit: `Stalchild.action.InterruptSmallHit` = ExecuteRagdollReactionProcess (+sweep-out); Die: `Stalchild.action.InterruptDie` = ExecuteRagdollReaction; no InterruptSmash module reachable | no (own RRC) | 30 | Ragdoll, petrify 0; also OneShotRagdollReactionEnd in utility root |
| Enemy_Keese x4 | yes | Keese(-1), Keese_WaitDynamic(-1) | no (Stun) | InterruptSmash (table Keese), Keese.interruptdie (ExecuteRagdollReaction) | no | 30 | Ragdoll, petrify 0 |
| Enemy_Tektite_Red/Blue | yes | Tektite_Red_FullDynamic(-1,c) | no | InterruptSmash, InterruptHammer, Slip; table Default | no | 30 | Ragdoll, petrify 0 |
| Enemy_Skulltula (+Truth) | yes | Skulltula_FullDynamic(-1) | no | InterruptSmash, Skulltula_Die; `ExecuteRagdollChangeLayer` in BattleWait | no | 45 | Ragdoll, petrify 0; IsDieRagdollReaction false |
| Enemy_LikeLike | yes | LikeLike_Die(-1,c) | no | die module -> ragdolldie | no (RRC none) | - | hash 3594736239, petrify 0 |
| Enemy_Biri, Bubble x5, Gohma_Larva, DeadHand (+Hand), SkullKid, Tailpasaran, TorchSlug, Peahat x2, Poe_Big, Tentacle x3, Dekubaba x4 | yes | FullDynamic(-1) or Die lists (Dekubaba: Dekubaba/Attack/Stun_St/Tongue_Anim; Tentacle: Attack_Time_Damage(50,c), N(45,c); DeadHand_Hand: SmallDamage lists 15-20) | Dekubaba GetUp (yes, non-ragdoll), Poe GetUp (yes, floating); others no | mostly the shared `Bokoblin.interruptsmash/die` ; Tentacle/DeadHand_Hand/IronKnuckle/Stalchild call ExecuteRagdollReactionProcess directly | DeadHand_Hand: yes; rest no | 30-60 | Ragdoll, petrify 0 for most |
| Enemy_MiddleBoss_DarkLink | yes (Player rig: DeadDown/DownLand/ImmediateDown/Pressed...) | Player_* | **yes** `Damage_Down_*_StandUp*` | own `...action.InterruptSmash` (table: Smash column in every row) | no | - | hash 4119866688 |
| Npc_SkullKid_Child(+_02,_03), Npc_SkullChild_001-005, Npc_SkullDad, Npc_Hylia_MSP_Default | yes | FullDynamic(-1) | SkullChild/Dad: Npc_Hylia_MN has Down+GetUp (NPC path `NpcCommon.utility.SimpleGetUp`, `NpcCommonSetDownState`) | NPC interrupt modules (`NpcCommon.action.interruptfall`, `ExecuteGetUpAfterDown`), not enemy ones | - | - | - |

Tables lookups I could not make: Enemy_Stalfos RRParam (GameParameterTable did not resolve), Haribote variants (no AI), Tentacle (no AS).

## Notes per actor

### Enemy_Lizalfos (also MiddleBoss_Lizalfos; Enemy_Dinolfos inherits ActorParam/AI/death/RRC from it)
- Files: `work/scan/u/Enemy_Lizalfos/Phive/Ragdoll/Reaction/Enemy_Lizalfos_{FullDynamic,SmallDamage}_List.*` (FullDynamic: NumReactionFrames -1, NumReactionEndFrames 15, StartWithCurrentPose false, StableThreshold 0.6; SmallDamage: 20 frames, StartWithCurrentPose true, EndFrames 10); AnimationDrive `Default` and `WhileAttacking` variants; Operation `Sword`.
- RRC: `Component/RagdollReactionControllerParam/Enemy_Lizalfos` -> parent `Enemy_Common_AutoRagdollReaction` (`IsEnableAutoRagdollReaction: true`), `IgnoreAutoReactionKeyPair.AnimationDriveKey 2675241661` (some animation-drive key excluded from auto reaction; hash not decoded).
- AS: `AS/Enemy_MiddleBoss_Lizalfos.root.asb` (ActorParam ASRef; `Enemy_Lizalfos.root.asb` also exists): commands Appear, AttackJump, DamageWhileAttacking, Die, Repelled, SmallDamage, Stun_*, Uproar...; clip names Damage_B, BigDamage_B/L/R, Die_B/L/R/F, Stun_*. **No DownWait/DownRecover/SmashDamage.** AsNode event archive has an `OverrideReaction` event (trigger for auto ragdoll, nodes not identified).
- AI: main root `Enemy_Lizalfos.action.root` lists InterruptSmash, InterruptSmallHit (Lizalfos.action.InterruptSmallHit), InterruptMediumHit/HugeHit (Bokoblin?/shared), InterruptDie (Bokoblin.interruptdie -> EnemyCommon.common.ragdolldie, `IsDieRagdollReaction` true), InterruptRepelled (Lizalfos.InterruptRepelled: plays `Repelled`, `OneShotRagdollReactionRequestDisableAutoReactionNext` = suppress auto ragdoll for the next frame/reaction), Cower (Lizalfos.Cower: `OneShotRagdollReactionEnd` + AS `BeforeEscape`: ends a running ragdoll reaction, used when scared).
- Hit path: `InterruptSeqParam` (`BSA/InterruptSeqParam/Enemy_Lizalfos`) -> `DamageReactionTable` `ExternalDamageReactionTable_Lizalfos` (= Default + Hookshot all None). Effective verbs: Strong/Smash intensity of Arrow/Beam/Body/Bomb/Fire/Impulse/LargeWeapon/Shield/SmallWeapon/ThrustWeapon -> `InterruptSmallHit`; None -> none; Weak/Moderate for sword types: no `ActionVerb` in the cell (engine default, guess = SmallHit/MediumHit by intensity); Hammer/Boomerang/Bottle/Slingshot: unspecified. `IsInterruptHammer` false.
- Death: `DeathRef Enemy_Lizalfos -> EnemyCommonRagdollDie` table: LifeDepleted DeathProcedureType `Ragdoll`, `PetrifiedStartFrame 0`, `PetrifiedTimeType Small`, `DeathExtendDownTimeType Small`, `CustomDeathStartTime 18`. `GameParameter/EnemyGlobalParam`: DeathExtendDownTimeSmall/Middle/Large = 5/25/30. "Petrified" = the gray stone fade (guess); `Destruction/GameDestructionExtraParam/DeathPetrified` exists in Peahat packs. `ExecuteASPlayWithDeathStartEffect`, `Die` AS command.
- RagdollReactionParam (`GameParameter/RagdollReactionParam/Enemy_Lizalfos` -> `Enemy_Common_Middle`): GroundDownTime 80, InWaterDownTime 80, IsEnableStableCheck false, RagdollReactionDownTime 0 (base Enemy_Common: 30/30).

### Enemy_Moblin
- List `Moblin_FullDynamic`(-1, EndFrames 5). RRC = EnemyCommon (auto false). AS `Enemy_Moblin.root.asb`: Stun_St/Lp/Ed, Die_B/R/L clips; no SmallDamage command. Table `NoReaction`. Modules: Moblin.interrupthookshot, interruptlightchemical + shared smash/die/smallhit. Death via `Enemy_Moblin_game__death__DeathReasonProcedures` (Ragdoll, petrify 0, ExtendDown Small), GroundDownTime 30.
- To knock down Moblin you must (a) change its table off `NoReaction` (guess: set `InterruptSeqParam.DamageReactionTable` in `BSA/InterruptSeqParam/Enemy_Moblin` or edit cells) and (b) accept that there is no hit-reaction clip.

### Enemy_Stalfos (+3 variants; Haribote has Dummy AS)
- Lists: `Stalfos_Damage`(-1), `Stalfos_ScatterBody`(-1, c, EndFrames 2), `Stalfos_SmallDamage_Middle`(30, EndFrames 30). There is **no list literally named FullDynamic** (guess: auto/smash would use another list name).
- RRC `Enemy_Common_AutoRagdollReaction` (auto true); AsNode `OverrideReaction` event present.
- ASB: BigDamageBack, Damage_*, Fall, GuardShock, Stun_* (no knock down/get up); ScatterBody = shatter into bones.
- AI: `Enemy_Stalfos.interruptseq.root` (own), `Stalfos.action.InterruptDie/Guard/Paralyze/SmallHit`, `Bokoblin.action.interruptsmash` and `interruptfall`, `InterruptDown` verb. Damage table `Stalfos` (Default + `EnergyWall`/`Spike` rows = InterruptSmash on Strong/Smash, MediumHit on Moderate; LargeWeapon/ThrustWeapon Weak/Moderate = MediumHit; Slingshot/Hookshot Weak = SmallHit).
- Death: table `Enemy_Stalfos_game__death__DeathReasonProcedures` (Ragdoll, petrify 0 / Small), `IsDieRagdollReaction false` (its InterruptDie still reaches ragdolldie).

### Enemy_Dinolfos
- Pack contains both Lizalfos and Dinolfos list files. ActorParam `$parent` = Enemy_Lizalfos, AS `Enemy_Dinolfos.root.asb` (LargeDamage, SmallDamage, Repelled, Die; no get-up). Own AI: `Enemy_Dinolfos.action.root` + Dinolfos.action.InterruptDie/InterruptRepelled, shares Lizalfos.Cower and InterruptSmallHit; `InterruptSeqParam` override: Intensity None -> InterruptHugeHit (hash 247104967). Auto reaction yes (inherited).

### Enemy_IronKnuckle_White/Black/Silver, Nabooru
- Lists: SmallDamage_Middle(15), BreakArmor(-1), DeathDropWeapon(-1). `Enemy_IronKnuckle.action.InterruptSmallHit` -> ExecuteRagdollReactionProcess with inputs DownTime, ExtendDownTime, FinishTimeOutCount, IsForceDown, IsEnableStable, ForceAnimationDriveKey, BreakArmor, and ASCommand SmallDamageFront/Back. `Enemy_IronKnuckle.action.StandUp` plays `StandUp` (anime-driven move, re-lock-on): the only enemy where a stand-up clip is wired next to a ragdoll node (hit and stand-up wiring order not verified - guess that StandUp follows IsForceDown knockdowns). Auto reaction yes. Variants inherit White's ActorParam.

### Enemy_Wolfos / Wolfos_White
- Only `Wolfos_Die` (-1) list; RRC `Enemy_Wolfos` = EnemyCommon default (auto false), but AS has the `OverrideReaction` event. AI: Wolfos.InterruptHugeHit/Repelled/Guard/Die modules (+ shared smash, never found a FullDynamic list). ASB: SmallDamage, MediumDamage, Damage_Back/M, Guard, Stun; no get-up. Table `Wolfos`: Arrow/Fire any intensity -> MediumHit.

### Enemy_ReDead / Gibdo
- Lists FullDynamic(-1), SmallDamage(15). Auto reaction true; GroundDownTime 900 frames. ASB: SmallDamage_Sit/Stand, Stun_*, Standing, SitDown; Gibdo also Regain, Sleep, WakeUp (coffin). Table `Redead`: everything None except Bomb (SmallHit) and Hookshot (verb Hookshot) (GibdoCoffin/RedeadConstraints are additional tables). `Redead.action.interruptdie` -> ragdolldie + `QueryRagdollReactionIsStable`, `QueryRagdollReactionIsCurrentReactionEnableVelocityDrive`, `ExecuteReDeadDieContactEffectEmit`. ReDead hits are normally only reacted to via Down/Sun song (`interruptsunofthesong`), Bomb, Hookshot.

### Enemy_Stalchild (+Big)
- Lists CutNeck, Die, ScatterBody_Hose, ScatterBody_NoNeck_Hose; `Stalchild.action.InterruptSmallHit` uses ExecuteRagdollReactionProcess, plus `OneShotSweepOutRequest*` (fade away / sweep out); `InterruptDie` uses ExecuteRagdollReaction with `RagdollReactionName`, RagDollAddDownTime, GroundDownTime. Table `Stalchild` (EnergyWall/Spike Strong+Smash -> InterruptSmash). No get-up: it disappears (`Disappear*` clips) (guess).

### Small enemies (Keese, Tektite, Skulltula, LikeLike, Biri, Bubble, Gohma_Larva, DeadHand, Tailpasaran, TorchSlug, Peahat, Poe_Big, Tentacle, Dekubaba, SkullKid)
- All reuse the shared roots: `Bokoblin.action.interruptsmash` / `interruptdie` -> `Common.action.ExecuteRagdollReaction`/`EnemyCommon.common.ragdolldie`. Keese: `Keese.action.interruptdie` and Guay variant call ExecuteRagdollReaction directly. Gohma_Larva: own `GohmaLarva.interruptsmash` and a table where almost every cell is InterruptSmash (the only normal enemy with a real smash-by-sword table). Peahat: Sitting/`WakeUp` = state, not ragdoll. Dekubaba: `interruptdowndamage` + `getup` real animated get-up (non-ragdoll). Enemy_Poe_Big: `PoeBig.action.getup`.

## Death: fade / gray

- DeathParam -> table `*_game__death__DeathReasonProcedures` (loose in `System/CombinationDataTableData/`): `LifeDepleted___Data` has `DeathProcedureType` (hash 3734144276 = **Ragdoll**; 'Default' hash for non-ragdoll; AI-die 4119866688; IronKnuckle/LikeLike/TorchSlug/Peahat 3594736239), `PetrifiedStartFrame` (0 = turn to stone from first death frame), `PetrifiedTimeType` (hash = **Small**, 2354307860), `DeathExtendDownTimeType` (Small), `CustomDeathStartTime` (Lizalfos 18), `IsWaitForSignal`, `IsRequestDestruction`. `LifeCondition` row: `PetrifiedStartFrame 7`; `UpperCrushed` row: 1.
- `GameParameter/EnemyGlobalParam/Default`: `DeathExtendDownTimeSmall/Middle/Large` = 5 / 25 / 30.
- Common AI names near it: `ExecuteDeathSignalDeathProcedure`, `QueryDeathGetDeathProcedureType`, `EnemyCommon.ExecuteASPlayWithDeathStartEffect` (AS Die + XLink `Death_Start`), `ExecuteGameBalanceReleaseMeleeBattlePermissionForDie`. The "gray" look is (guess) Petrified: changing `PetrifiedStartFrame` / `PetrifiedTimeType` in the Lizalfos' table `EnemyCommonRagdollDie_game__death__DeathReasonProcedures` (Lizalfos/Dinolfos) or `Enemy_Moblin_game__death__DeathReasonProcedures` is the data-only handle.

## Damage types that trigger smash knockdown

Intensity columns of `ExternalDamageReactionTable`: `None, Weak, Moderate, Strong, Smash` (+`Super` in ImpulseDamageParameters). Rows (damage type): Arrow, Beam, Body, Bomb, Boomerang, Bottle, Durability, Fire, Hammer, Hookshot, Impulse, LargeWeapon, Shield, Shockwave, SlingshotBullet, SmallWeapon, Spike, ThrustWeapon, Undefined (+EnergyWall in Stalfos/Stalchild/Peahat tables). Cell = `ActionVerb` as murmur3_32 hash of the name (decoded: 322480722 InterruptSmallHit, 3322560101 **InterruptSmash**, 2880891034 InterruptMediumHit, 247104967 InterruptHugeHit, 1582167558 InterruptDown, 2703061742 InterruptDownDamage, 3069107721 None, 3390960474 InterruptGuard, 4066352561 InterruptBoomerang, 1183302887 InterruptHookshot, 3196207016 InterruptWeakPointHit, 219895878 InterruptCriticalHit, 1182204697 InterruptStruggle, 79766591 InterruptSpecialAction01, 1036390309 InterruptStealthHit). Intensities from Link's attacks are not decoded here (Player pack not scanned).

| Table | InterruptSmash cells |
|---|---|
| `Default` (Lizalfos, Dinolfos, Wolfos, IronKnuckle, LikeLike, Bubble, Tektite...) | none. Strong+Smash on Arrow/Beam/Body/Bomb/Fire/Impulse/Large/Small/Thrust/Shield = InterruptSmallHit; Weak/Moderate mostly empty cell |
| `Stalfos`, `Stalchild`, `Peahat` | `Spike` and `EnergyWall` x {Strong, Smash} (Moderate = MediumHit, Weak = SmallHit) |
| `DarkLink` | every row x Smash |
| `Gohma_Larva` | nearly every row x every intensity |
| `NoReaction` (Moblin, Bubble_Green_Big, Peahat default) | all None |

Engine-side numbers: `GameParameter/KnockBackParam/Default`: `ExtraSmashRate{Small,Large,Thrust}Weapon = 2.0` (guess: multiplier that promotes a weapon hit to Smash intensity); `InterruptSeqParam.InterruptSmashSetting` (`IsDropEquipmentSmash`, `DropEquipmentSmashLifePercent 0.5`); RRC `SmashIntensityRate` (array, unreadable with byml.py, node type 0x20); `DamageParameters.ImpulseDamageParameters` (Weak 4 / Moderate 8 / Strong 12 / Smash 16, `IsImpulseDamageEnabled` false for Lizalfos).

## Hook points for "any hit = knockdown with get-up"

1. **Data (romfs override)**: edit `System/CombinationDataTableData/game__life__ExternalDamageReactionTable_Lizalfos.bgyml` (parent `..._Default`) and add cells for the hitting rows (SmallWeapon, LargeWeapon, ThrustWeapon, Body, Arrow, ...) x columns with `ActionVerb: 3322560101` (InterruptSmash). The Lizalfos root already has InterruptSmash wired, so the smash module runs: `Common.action.smash` + `ExecuteRagdollReaction` (`FullDynamic`, which Lizalfos/Moblin have). Moblin additionally needs another table than `NoReaction` (set in `BSA/InterruptSeqParam/Enemy_Moblin` inside the actor pack). Needs patched packs or loose-file override; the table cells are loose files in romfs, so no pack rewrite for Lizalfos/Stalfos/Wolfos tables.
2. Get-up behaviour: ragdoll lasts `GroundDownTime`/DownTime frames (Lizalfos 80, others 30) and is then handed back to the AI; no stand-up clip exists (point 1 above), so there will be a pose snap (guess). To get a real animated get-up you would need a clip: only IronKnuckle (`StandUp`), Dekubaba and Poe have one.
3. **Runtime**: hook the enemy `RagdollReactionController` start `main+0x2e916b0(comp, Req*)` (see FINDINGS) after the damage resolves, or force the BSA interrupt verb to `InterruptSmash` (hash 3322560101) where `SelectorBSAInterruptAction` consumes it; or enable `IsEnableAutoRagdollReaction` plus the AS `OverrideReaction` event on the actors' hit clips (Lizalfos/Dinolfos/Stalfos/ReDead/IronKnuckle already have it).
4. Strong candidate for lowest risk: option 1 for Lizalfos (user's test save) plus a live test that `DownTime` returns control (watch `QueryIsNowRagdoll` state at comp+0x20 = 3 -> 0).

## Not done / open

- AINB node wiring (conditions between interrupt verbs, `DownTime` default values, whether `ExecuteRagdollReaction` falls back to Wait when `DownRecover` is missing) not parsed: names only.
- Where intensity (Weak..Smash) of Link's hits is set (Player attack params), and which `Intensity` an empty table cell resolves to.
- Which AS nodes carry `OverrideReaction` (event archive keys not decoded) and what animation-drive key 2675241661 is.
- Actors with no ASRef/AI of their own (IronKnuckle Black/Silver via parent, Haribote, Tentacle Blue/Green) only analysed via the parent's data.
