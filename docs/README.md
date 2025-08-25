## Compatibility list

The compatibility list is described in a YAML file that contains information
concerning per-addin emulation. The whole file is organized as follow:

| Name     | Type              | Description              |
|:--------:|:-----------------:|--------------------------|
| `addins` | `dict[str,Addin]` | dictionary of all addins |

Note that I will use the `python` type style for all documented tables, an
optional question mark is added to the name to indicate that this field is
optional. If you find a type that is not basic (e.g `str` or `list[str]`) it
will normally be described after.

### `Addin` declaration

The most used description since it describe each addin information

| Name           | Type        | Description                                |
|:--------------:|:-----------:|--------------------------------------------|
| `author`       | `str`       | author's name                              |
| `status`       | `Status`    | emulation status (see below)               |
| `description`  | `str`       | short description of the addin             |
| `homepage`     | `str`       | URL of the project topic                   |
| `updated`      | `str`       | date of the last check update              |
| `repository`?  | `str`       | URL of the `git` repository                |
| `device`?      | `Device`    | emulation device (see below)               |
| `sha256`?      | `list[str]` | sha256 of all addin file that refers to it |
| `patches`?     | `list[str]` | list of patch that can be applied          |
| `states`?      | `list[str]` | list of information concerning emulation   |

### `Device` table

This table is only a view on all device name supported in `Device` field
(this is not a dictionary)

| Name     | Description                                      |
|:--------:|--------------------------------------------------|
| `fxcg50` | Emulate the `fxcg50` device with firmware `3.80` |
| `fx9860` | Emulate the `fx9860` device with firmware `2.05` |

### `Status` table

Same as `Device` table, only here to view all possible "status" available.
This is almost the same as used in the `RPCS3` emulator.

| Name       | Description                                      |
|:----------:|--------------------------------------------------|
| `playable` | can be completed with playable performance and no game breaking glitches |
| `ingame`   | either can't be finished, have serious glitches or have insufficient performance
| `intro`    | display image but don't make it past the menus   |
| `loadable` | display a black screen                           |
| `nothing`  | don't initialize properly, not loading at all and/or crashing the emulator |
