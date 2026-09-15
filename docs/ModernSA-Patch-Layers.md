# Modern SA patch layers

The Modern SA branch deliberately separates compatibility work into three layers.

## 1. Native engine fixes

These fixes are implemented in gta-reversed source code and replace original game behaviour at the reversed function level. They belong in normal engine files such as `CTimer`, input, rendering, streaming, vehicles, audio, scripts, and platform code.

Examples already migrated or started natively:
- fractional-millisecond `CTimer` accumulation for high refresh rates
- configurable high-refresh target (180 FPS default)
- desktop refresh-rate detection
- Windows high-resolution timer setup
- per-monitor DPI awareness
- focus-aware cursor clipping

Migration rule: when a SilentPatch behaviour is reproduced and validated natively, mark the external equivalent as redundant and avoid double patching.

## 2. SilentPatch fallback

Official SilentPatch SA is kept as a separate external compatibility layer for fixes whose target functions are not yet reversed, whose original binary path still owns the behaviour, or whose source-level port has not yet been validated.

This layer must remain independently installable and removable. It must never be presented as a native gta-reversed engine fix.

Target state: shrink this layer as native ports become complete and tested.

## 3. Map / PS2 / content assets

Map, model, IDE, IPL, TXD, timecycle, and other content corrections are data assets, not engine patches. They stay in a separate optional content layer and should normally be installed through the layout required by their originating project (for example ModLoader) instead of being blindly merged into `gta3.img`.

## Packaging

CI publishes three independent artifacts:
- `gta-reversed-EngineModernization`: native gta-reversed engine changes only
- `gta-reversed-SilentPatch-Fallback`: official SilentPatch fallback only
- `gta-reversed-ModernSA-FullCompatibility`: combined install-ready package

## Porting policy

For every SilentPatch fix considered for migration:
1. identify the original GTA SA function/address and the corresponding gta-reversed source path;
2. verify the original behaviour and the SilentPatch correction;
3. implement the fix in the reversed function rather than adding a second binary hook whenever practical;
4. build and regression-test at 30/60/120/144/165/180+ FPS where timing is relevant;
5. only then disable/remove the overlapping external fallback for that fix.

This keeps the engine work reviewable and avoids two patches fighting over the same code path.
