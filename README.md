# AI Cleaner

Plugin d'effet (VST3 + AU) qui nettoie les stems générés par IA, comme ceux de Suno : aigus métalliques, sifflantes, résonances, son boueux, repisse (bleed), reverb, stéréo qui phase et attaques molles.

**Created by MAAURA ([@maaura.music](https://www.instagram.com/maaura.music/)) and DANIDANE ([@danidane.music](https://www.instagram.com/danidane.music/))**

---

## Télécharger

Va dans **Releases** (colonne de droite de cette page GitHub) et prends la version la plus récente :

| Système | Fichier |
|---|---|
| Mac (Apple Silicon et Intel) | `AI-Cleaner-Mac-Installer.pkg` |
| Windows 10/11 (64 bits) | `AI-Cleaner-Windows-Setup.exe` |

## Installer sur Mac

1. Double-clique sur `AI-Cleaner-Mac-Installer.pkg`.
2. Si macOS affiche « impossible de vérifier le développeur » : ouvre **Réglages Système › Confidentialité et sécurité**, descends en bas et clique sur **Ouvrir quand même**, puis relance le fichier. C'est normal, le plugin n'est pas signé par Apple (ça demande un compte développeur payant).
3. Clique sur **Continuer** jusqu'à la fin.
4. Dans FL Studio : **Options › Manage plugins › Find plugins**, puis cherche **AI Cleaner** dans les effets.

Le plugin est installé dans `/Library/Audio/Plug-Ins/VST3` et `/Library/Audio/Plug-Ins/Components`.

## Installer sur Windows

1. Double-clique sur `AI-Cleaner-Windows-Setup.exe`.
2. Si Windows affiche « Windows a protégé votre ordinateur » : clique sur **Informations complémentaires › Exécuter quand même**.
3. Suis l'installation. Le plugin va dans `C:\Program Files\Common Files\VST3`.
4. Dans FL Studio : **Options › Manage plugins › Find plugins**.

## Utilisation rapide

1. Mets **AI Cleaner** sur la piste du stem.
2. Choisis le type de stem en haut : **Vocals, Drums, Bass, Instruments, Full Mix**.
3. Glisse le **cœur** vers le haut ou le bas : il règle les 8 modules en même temps.
4. Clique sur **Listen: removed** pour entendre ce que le plugin enlève. Si tu entends de la musique dedans, baisse le cœur.
5. Les **coupes de fréquences** sont en **AUTO** par défaut. Attrape un point pour les régler toi-même.

Astuces : double-clic sur un bouton = valeur par défaut. Maj + glisser sur le cœur = réglage fin. La fenêtre se redimensionne par le coin en bas à droite.

## Les modules

| Module | Ce qu'il fait |
|---|---|
| Metallic Highs | Calme le scintillement « métallique » / « eau » dans les aigus |
| Sibilance | Adoucit les S et les T |
| Resonance | Calme les fréquences qui sonnent et sifflent |
| Mud | Nettoie les bas-médiums flous (comme un EQ qui s'adapte tout seul) |
| Bleed | Réduit la repisse des autres instruments |
| Deverb | Réduit les queues de reverb |
| Stereo Fix | Stabilise la stéréo et met le grave en mono |
| Punch | Redonne de l'attaque aux transitoires |

Le plugin ajoute une petite latence (environ 43 ms à 48 kHz), compensée automatiquement par FL Studio et les autres logiciels.

---

## Pour les développeurs

Construit avec [JUCE 8](https://juce.com) et CMake. GitHub Actions fabrique automatiquement les plugins et les installateurs à chaque envoi sur `main` (voir `.github/workflows/build.yml`).

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Polices : Anton et Outfit (SIL Open Font License, voir `Resources/Fonts`).
