#pragma once

#include <Arduino.h>
#include <esp_mac.h>

// Original word lists for deterministic setup-AP names. Keep entries short,
// ASCII-only, and friendly: the resulting SSID must fit Wi-Fi's 32-byte limit.
namespace ApNames {

static constexpr size_t WORD_COUNT = 128;

static const char *const ADJECTIVES[WORD_COUNT] = {
  "Bouncy", "Brave", "Breezy", "Bright", "Bubbly", "Clever", "Cozy", "Cosmic",
  "Cuddly", "Dapper", "Dizzy", "Dreamy", "Eager", "Fluffy", "Friendly", "Fuzzy",
  "Goofy", "Happy", "Hasty", "Jolly", "Jumpy", "Kind", "Lively", "Lucky",
  "Merry", "Mighty", "Misty", "Nifty", "Peppy", "Perky", "Pinky", "Plucky",
  "Puffy", "Quirky", "Rosy", "Rusty", "Sassy", "Shiny", "Silly", "Sleepy",
  "Snappy", "Snug", "Sparkly", "Spiffy", "Spry", "Squishy", "Sunny", "Swift",
  "Tasty", "Tiny", "Toasty", "Tricky", "Twinkly", "Wacky", "Waggly", "Warm",
  "Wiggly", "Witty", "Wobbly", "Woozy", "Zany", "Zippy", "Able", "Amusing",
  "Bashful", "Beaming", "Blinky", "Blooming", "Breezy", "Bristly", "Bubbly", "Chirpy",
  "Dandy", "Dewy", "Doting", "Dusky", "Feisty", "Floppy", "Fluttery", "Frosty",
  "Giggly", "Glimmering", "Huggable", "Jazzy", "Jelly", "Kooky", "Loopy", "Lunar",
  "Mellow", "Mochi", "Muddy", "Mushy", "Noodly", "Peachy", "Pickly", "Pipin",
  "Plush", "Pompom", "Pudding", "Puzzled", "Quacky", "Rambly", "Round", "Scribbly",
  "Shy", "Skippy", "Slinky", "Smiley", "Smol", "Snorkely", "Sparky", "Sprouty",
  "Starry", "Stout", "Stretchy", "Tippy", "Toothy", "Velvety", "Waddly", "Whimsy",
  "Wiggly", "Windy", "Winky", "Yummy", "Zesty", "Zoomy", "Zucchini", "Zumbly"
};

static const char *const NOUNS[WORD_COUNT] = {
  "Alpaca", "Badger", "Biscuit", "Bumblebee", "Capybara", "Chickpea", "Corgi", "Cricket",
  "Dumpling", "Ferret", "Fig", "Finch", "Fox", "Frog", "Goose", "Hamster",
  "Hedgehog", "Koala", "Lemur", "Marmot", "Mochi", "Mongoose", "Mouse", "Muffin",
  "Noodle", "Otter", "Pancake", "Penguin", "Pickle", "Pigeon", "Puffin", "Raccoon",
  "Ravioli", "Robin", "Seagull", "Sloth", "Snail", "Sparrow", "Spud", "Squirrel",
  "Taco", "Toad", "Turtle", "Waffle", "Walrus", "Wombat", "Yak", "Zebra",
  "Acorn", "Avocado", "Banana", "Bean", "Berry", "Bloop", "Bunny", "Button",
  "Cactus", "Carrot", "Cashew", "Cloud", "Coconut", "Cookie", "Cornflake", "Cupcake",
  "Dandelion", "Donut", "Eclair", "Feather", "Firefly", "Flapjack", "Gizmo", "Grape",
  "Jellybean", "Kettle", "Kiwi", "Loaf", "Marble", "Marshmallow", "Meerkat", "Moonbean",
  "Nugget", "Omelet", "Pancake", "Pawpaw", "Peanut", "Pebble", "Pecan", "Popsicle",
  "Potato", "Pretzel", "Pumpkin", "Quokka", "Radish", "Samosa", "Sausage", "Sprinkle",
  "Starfish", "Tater", "Teacup", "Tofu", "Tomato", "Turnip", "Wiggle", "WonTon",
  "Yeti", "Zucchini", "Bumble", "Button", "Doodle", "Fluffball", "Goober", "Hiccup",
  "Jiggle", "Kibble", "Loopy", "Mittens", "Nibbler", "Pogo", "RolyPoly", "Scooter"
};

static_assert(sizeof(ADJECTIVES) / sizeof(ADJECTIVES[0]) == WORD_COUNT,
              "adjective list must contain exactly WORD_COUNT entries");
static_assert(sizeof(NOUNS) / sizeof(NOUNS[0]) == WORD_COUNT,
              "noun list must contain exactly WORD_COUNT entries");

inline bool makeName(char *out, size_t outLen) {
  uint8_t baseMac[6] = {};
  if (esp_base_mac_addr_get(baseMac) != ESP_OK || outLen == 0) {
    if (outLen > 0) out[0] = '\0';
    return false;
  }

  // Mix different bytes so nearby MACs do not simply walk one list in lockstep.
  const size_t adjective = ((uint16_t)baseMac[0] << 8 | baseMac[5]) % WORD_COUNT;
  const size_t noun = ((uint16_t)baseMac[2] << 8 | baseMac[3]) % WORD_COUNT;
  const int written = snprintf(out, outLen, "SHS-%s-%s",
                               ADJECTIVES[adjective], NOUNS[noun]);
  return written > 0 && (size_t)written < outLen;
}

}  // namespace ApNames
