# Kenshi Plugins

RE_Kenshi / KenshiLib plugins for Kenshi 1.0.65 / 1.0.68.

| Plugin | What it does | Workshop |
|---|---|---|
| **GateFix** | Characters open closed gates on their own, pass through, and the gate closes and re-locks behind them | [link](#) |
| **SleepFix** | Wounded characters take themselves to bed and return to work once healed | [link](#) |
| **CorpseLoot** | Recovers inventory and worn equipment from corpses before disposal destroys them | [link](#) |
| **FurnaceSlots** | Keeps furnaces switched on and reduces the frame hitch from long item lists | [link](#) |

Each plugin is a single .cpp file. No third-party libraries beyond KenshiLib.

## Requirements

- [RE_Kenshi](https://www.nexusmods.com/kenshi/mods/847) 0.3.x or newer
- [KenshiLib](https://github.com/BFrizzleFoShizzle/KenshiLib)

## Building

- Visual Studio 2022 with the **VS2010 (v100) toolset**, x64, Release
- Link against the prebuilt `KenshiLib.lib` and `OgreMain.lib`
- Add KenshiLib's `Include` directory and Boost 1.60.0 headers to your include paths

You do not need to compile RE_Kenshi or KenshiLib — precompiled versions are available
in the [KenshiLib_Examples_deps](https://github.com/BFrizzleFoShizzle/KenshiLib_Examples_deps) repo.

The include paths in the .vcxproj files point at my own install. Adjust them to match yours.

## Notes for plugin developers

These cost me time — maybe they'll save you some.

**Do not put `extern "C"` on the entry point.** RE_Kenshi looks it up by the mangled
name `?startPlugin@@YAXXZ`. With `extern "C"` the DLL still loads, but initialisation
silently fails and your plugin never runs.

```cpp
__declspec(dllexport) void startPlugin();   // declare
void startPlugin() { ... }                  // define
```

**Don't call `_NV_` versions of virtual functions.** `_NV_isAnimal`, `_NV_isGate` and
friends are that class's own implementation, not a virtual dispatch. Calling
`Character::_NV_isAnimal()` on an animal returns NULL, because `CharacterAnimal`'s
override is skipped. Call through the vtable slot instead, and verify the slot index
by comparing it against the `_NV_` function's real address on an object that does *not*
override it.

**Door orders need `clear=0`.** `addOrder(dest, task, subject, shift, clear, pos)` for
door tasks (72 open / 73 close / 77 lock / 140 unlock) must be `dest=door, shift=0,
clear=0`, with the **door leaf** (`DoorStuff*`) as the subject — not the gate building.
With `clear=1` the order is issued but the character never acts on it, at any distance.

**One click is an order storm.** A single right-click produces dozens to hundreds of
move orders, all with `clear=1`. Anything you inject during the storm gets wiped. Wait
until the storm goes quiet before injecting.

**Measure state changes after they've had time to happen.** Reading a door's open
amount immediately after calling `closeDoor()` always returns the old value — the
animation hasn't started. I reversed the same verdict twice because of this.

**Check whether the game already does it.** I spent three attempts making animals walk
to a gate on their own before realising a vanilla Follow job already solves it.

## License

GPLv3. KenshiLib is GPLv3, and its author notes that plugins linking against it must
be GPLv3 as well.

## Credits

RE_Kenshi and KenshiLib by [BFrizzleFoShizzle](https://github.com/BFrizzleFoShizzle).
