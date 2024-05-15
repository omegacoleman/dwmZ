#pragma once

#include "bar/gc/colorscheme.hpp"

namespace bar {

namespace colors::kanagawa {
using namespace bar::literals;

// Bg Shades
constexpr auto sumiInk0 = "#16161D"_rgb;
constexpr auto sumiInk1 = "#181820"_rgb;
constexpr auto sumiInk2 = "#1a1a22"_rgb;
constexpr auto sumiInk3 = "#1F1F28"_rgb;
constexpr auto sumiInk4 = "#2A2A37"_rgb;
constexpr auto sumiInk5 = "#363646"_rgb;
constexpr auto sumiInk6 = "#54546D"_rgb; // fg

// Popup and Floats
constexpr auto waveBlue1 = "#223249"_rgb;
constexpr auto waveBlue2 = "#2D4F67"_rgb;

// Diff and Git
constexpr auto winterGreen = "#2B3328"_rgb;
constexpr auto winterYellow = "#49443C"_rgb;
constexpr auto winterRed = "#43242B"_rgb;
constexpr auto winterBlue = "#252535"_rgb;
constexpr auto autumnGreen = "#76946A"_rgb;
constexpr auto autumnRed = "#C34043"_rgb;
constexpr auto autumnYellow = "#DCA561"_rgb;

// Diag
constexpr auto samuraiRed = "#E82424"_rgb;
constexpr auto roninYellow = "#FF9E3B"_rgb;
constexpr auto waveAqua1 = "#6A9589"_rgb;
constexpr auto dragonBlue = "#658594"_rgb;

// Fg and Comments
constexpr auto oldWhite = "#C8C093"_rgb;
constexpr auto fujiWhite = "#DCD7BA"_rgb;
constexpr auto fujiGray = "#727169"_rgb;

constexpr auto oniViolet = "#957FB8"_rgb;
constexpr auto oniViolet2 = "#b8b4d0"_rgb;
constexpr auto crystalBlue = "#7E9CD8"_rgb;
constexpr auto springViolet1 = "#938AA9"_rgb;
constexpr auto springViolet2 = "#9CABCA"_rgb;
constexpr auto springBlue = "#7FB4CA"_rgb;
constexpr auto lightBlue = "#A3D4D5"_rgb; // unused yet
constexpr auto waveAqua2 = "#7AA89F"_rgb; // improve lightness: desaturated greenish Aqua

constexpr auto springGreen = "#98BB6C"_rgb;
constexpr auto boatYellow1 = "#938056"_rgb;
constexpr auto boatYellow2 = "#C0A36E"_rgb;
constexpr auto carpYellow = "#E6C384"_rgb;

constexpr auto sakuraPink = "#D27E99"_rgb;
constexpr auto waveRed = "#E46876"_rgb;
constexpr auto peachRed = "#FF5D62"_rgb;
constexpr auto surimiOrange = "#FFA066"_rgb;
constexpr auto katanaGray = "#717C7C"_rgb;

constexpr auto dragonBlack0 = "#0d0c0c"_rgb;
constexpr auto dragonBlack1 = "#12120f"_rgb;
constexpr auto dragonBlack2 = "#1D1C19"_rgb;
constexpr auto dragonBlack3 = "#181616"_rgb;
constexpr auto dragonBlack4 = "#282727"_rgb;
constexpr auto dragonBlack5 = "#393836"_rgb;
constexpr auto dragonBlack6 = "#625e5a"_rgb;

constexpr auto dragonWhite = "#c5c9c5"_rgb;
constexpr auto dragonGreen = "#87a987"_rgb;
constexpr auto dragonGreen2 = "#8a9a7b"_rgb;
constexpr auto dragonPink = "#a292a3"_rgb;
constexpr auto dragonOrange = "#b6927b"_rgb;
constexpr auto dragonOrange2 = "#b98d7b"_rgb;
constexpr auto dragonGray = "#a6a69c"_rgb;
constexpr auto dragonGray2 = "#9e9b93"_rgb;
constexpr auto dragonGray3 = "#7a8382"_rgb;
constexpr auto dragonBlue2 = "#8ba4b0"_rgb;
constexpr auto dragonViolet = "#8992a7";
constexpr auto dragonRed = "#c4746e"_rgb;
constexpr auto dragonAqua = "#8ea4a2"_rgb;
constexpr auto dragonAsh = "#737c73"_rgb;
constexpr auto dragonTeal = "#949fb5"_rgb;
constexpr auto dragonYellow = "#c4b28a";

constexpr auto lotusInk1 = "#545464"_rgb;
constexpr auto lotusInk2 = "#43436c"_rgb;
constexpr auto lotusGray = "#dcd7ba"_rgb;
constexpr auto lotusGray2 = "#716e61"_rgb;
constexpr auto lotusGray3 = "#8a8980"_rgb;
constexpr auto lotusWhite0 = "#d5cea3"_rgb;
constexpr auto lotusWhite1 = "#dcd5ac"_rgb;
constexpr auto lotusWhite2 = "#e5ddb0"_rgb;
constexpr auto lotusWhite3 = "#f2ecbc"_rgb;
constexpr auto lotusWhite4 = "#e7dba0"_rgb;
constexpr auto lotusWhite5 = "#e4d794"_rgb;
constexpr auto lotusViolet1 = "#a09cac"_rgb;
constexpr auto lotusViolet2 = "#766b90"_rgb;
constexpr auto lotusViolet3 = "#c9cbd1"_rgb;
constexpr auto lotusViolet4 = "#624c83"_rgb;
constexpr auto lotusBlue1 = "#c7d7e0"_rgb;
constexpr auto lotusBlue2 = "#b5cbd2"_rgb;
constexpr auto lotusBlue3 = "#9fb5c9"_rgb;
constexpr auto lotusBlue4 = "#4d699b"_rgb;
constexpr auto lotusBlue5 = "#5d57a3"_rgb;
constexpr auto lotusGreen = "#6f894e"_rgb;
constexpr auto lotusGreen2 = "#6e915f"_rgb;
constexpr auto lotusGreen3 = "#b7d0ae"_rgb;
constexpr auto lotusPink = "#b35b79"_rgb;
constexpr auto lotusOrange = "#cc6d00"_rgb;
constexpr auto lotusOrange2 = "#e98a00"_rgb;
constexpr auto lotusYellow = "#77713f"_rgb;
constexpr auto lotusYellow2 = "#836f4a"_rgb;
constexpr auto lotusYellow3 = "#de9800"_rgb;
constexpr auto lotusYellow4 = "#f9d791"_rgb;
constexpr auto lotusRed = "#c84053"_rgb;
constexpr auto lotusRed2 = "#d7474b"_rgb;
constexpr auto lotusRed3 = "#e82424"_rgb;
constexpr auto lotusRed4 = "#d9a594"_rgb;
constexpr auto lotusAqua = "#597b75"_rgb;
constexpr auto lotusAqua2 = "#5e857a"_rgb;
constexpr auto lotusTeal1 = "#4e8ca2"_rgb;
constexpr auto lotusTeal2 = "#6693bf"_rgb;
constexpr auto lotusTeal3 = "#5a7785"_rgb;
constexpr auto lotusCyan = "#d7e3d8"_rgb;
}

}
