AREA51XR GAME MEDIA FOLDER
==========================

Area51XR does not include the original Area 51 arcade game, ROMs, CHDs, or other copyrighted game assets.

Use game media that you are legally entitled to use.

Default loose-file layout:

media\
  area51\
    2-c_area_51_hh.hh
    2-c_area_51_hl.hl
    2-c_area_51_lh.lh
    2-c_area_51_ll.ll
    jagwave.rom
    area51.chd

An existing compatible MAME media directory can also be used by launching Start-Area51XR.ps1 with:

  -RomPath "D:\path\to\your\media"

Area51XR verifies the media with the bundled emulator before starting VR. If verification fails, Area51XR will not launch the game.

If your valid files use different names, point Start-Area51XR.ps1 at the folder that contains them. Area51XR recognizes only exact supported content signatures, stages canonical copies under your local Area51XR data folder, and never renames, overwrites, or modifies the originals. Missing, invalid, or ambiguous files are rejected.
