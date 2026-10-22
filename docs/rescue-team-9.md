# Rescue Team 9 reference

The active reference for new engine work is **Rescue Team: Evil Genius
Collector's Edition (Rescue Team 9)**. Local retail files are kept in
`reference/rescue-team-9/`, which is ignored by Git.

The Windows installer in Downloads is
`Rescue-Team-9-Evil-Genius-Collectors-Edition.exe`. Extract it without running it:

```sh
innoextract --output-dir reference/rescue-team-9 \
  "$HOME/Downloads/Rescue-Team-9-Evil-Genius-Collectors-Edition.exe"
```

The game is Unity based. Its data is in `app/Data/data.unity3d`, with managed
assemblies in `app/Data/Managed/`. It does **not** use RT5's `texture/lazy/*.lua`
atlases or version 301 level files. Passing this directory to the RT5 importer
is insufficient; the older importer remains available for existing missions.

Do not assume Retina support from the release number. Inspect the player
settings, source texture sizes, sprite rectangles and canvas scaling before
adopting its rendering conventions.
