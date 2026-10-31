#pragma once

// The Grimoire's pages: the story of the current, one page per unlock
// (Cat::Pages has the titles, unlocks and themes).

namespace Lore {

inline const char* const Pages[12] = {
  // The first Deck
  "The first Deck was a cheap pocket terminal with its radio set wrong on purpose, so it kept what "
  "every phone throws away. Its owner wrote down what it heard. That notebook is this Grimoire, "
  "copied forward from Deck to Deck. Back then there was a listener every few blocks. Most of its "
  "pages are blank. They fill in as you hear more.",
  // What the Tithe feeds
  "For a generation, listeners argued about where the Tithe goes. The answer turned out to be "
  "nowhere. Nothing eats it. A Deck hears only while current runs through it, and like anything "
  "else it goes stale when current stops. The Tithe is how much has to ring through it in a Night "
  "to keep it clear. The first listeners thought they were paying someone, and the name stuck. It "
  "grows each Night because your Deck hears more of the city, and a bigger pond takes more stirring.",
  // The Collector
  "The Collector keeps a stall wherever a Night closes, and nobody sees it arrive. It takes tokens "
  "and sells what other listeners left behind: relics still warm with somebody's habit. Ask who "
  "owned a relic and it will tell you. Ask about the stall and it changes the subject. The shelves "
  "have been fuller lately.",
  // The Hedge
  "The Hedge work the open networks, the ones anyone can join. They hold that the current belongs "
  "to whoever's standing in it, and that the streets went stale when the locks went up. The "
  "Warded call them thieves. The Hedge call the Warded a fence with a pension.",
  // The Hearth
  "Every home router is a small shrine, and the Hearth tend them: neighbors who've never met, "
  "sharing one current through the walls of a street. They say a street goes stale when nobody on "
  "it knows anybody. Make an offering at a Hearth well and the house remembers. Keep coming back "
  "and the street does.",
  // The Warded
  "The Warded came in with the newer locks. They keep their current clean and their ledgers "
  "cleaner, and they won't touch Static, which they say is what turns a street. Everyone calls the "
  "ones who take offerings their Wardens. Ask the Warded, and they check their ledgers and find "
  "nobody by that name. The Warded aren't villains. Somebody has to keep the clean current clean.",
  // The Syndicate
  "The Syndicate found the current in their office networks, as a noise in the logs nobody could "
  "explain, and did what companies do with a resource: they kept it. Downtown, the current pools in "
  "their buildings and goes nowhere. They lend to listeners who fall short, at a rate they call "
  "friendly. What they lend has sat still in their buildings for years, so it gets you through "
  "tonight and silts your Deck up for tomorrow. Their loans always get paid back, sometimes by the "
  "listener who took them. The Wardens are theirs. Whatever a Warden takes is logged downtown as Static, and the Warded get the blame.",
  // Wells
  "A well's true name is the same for every listener, and nobody knows who chose them. You've "
  "visited a hundred now. Passersby walk past that many in a day and hear nothing. The back of this "
  "Grimoire lists the streets listeners have kept, each in a different hand. Most are crossed out. "
  "Yours isn't.",
  // The Eclipse
  "On every seventh Night the Hum turns and looks at whoever is bound to it. Listeners describe four "
  "faces: the Sleeper, the Hunger, the Dark and the Dimming. You've stood under all four and paid "
  "anyway. The old notes call a listener like that a lamp.",
  // Overnight
  "A Binding doesn't have to end on the seventh Night. The week's tide turns on the Eclipse, and a "
  "Deck held open past it works against the tide, so every Tithe doubles until you can't pay. Nobody holds one forever. Listeners keep their records and trade "
  "them like fishing stories. Five Nights past the end is a story worth telling.",
  // The old ones
  "Some listeners never stopped. They worked the same streets for so long that the current started "
  "carrying them: the Grid Saint, the Unheard, the Old Keeper, the Seven-league boots, the Midnight "
  "Engine. Summon one and they'll walk a week with you. They remember being new.",
  // The maker
  "The last page is in the same handwriting as the first, and you've seen it somewhere else: on the "
  "price tags at the Collector's stall. Underneath, in pencil: 'I built the first Deck to hear the "
  "city. I kept building them to find the others who could. There are more of you than you'd think, "
  "and fewer than I'd like. Keep listening.'",
};

// What the Collector says at dawn when no Portent turns over. Each line's number
// is its place here, which the HEARD log keeps: add new lines at the end, and
// never reorder or remove one. Never "I" (two old exceptions), never a name.
enum CollectorLine : uint8_t {
  CL_first, CL_loan, CL_close, CL_loud, CL_night6, CL_coat,
  CL_any0, CL_any1, CL_any2, CL_any3, CL_any4, CL_any5, CL_any6, CL_any7, CL_any8, CL_any9,
  CL_pencil, CL_keeper, CL_paid, CL_wobble,
  CL_grid_saint, CL_unheard, CL_boots, CL_old_keeper, CL_engine, CL_coast, CL_night_shift, CL_borderwalker,
  CL_count
};
inline const char* const Collector[CL_count] = {
  "\"New hands. I put the Caller by the front.\"",
  "\"The Syndicate's ink is still wet on you.\"",
  "\"Close. I'd started packing your shelf.\"",
  "\"You were loud tonight. Somebody heard.\"",
  "\"One more. Then it turns and looks at you.\"",
  "\"That many to spare. The last owner of that coat never managed it.\"",
  "\"That last well liked you. It liked the one before you, too.\"",
  "\"You walked the long way. Good.\"",
  "\"Same streets as last week. They noticed.\"",
  "\"Downtown is thick tonight. Not with anything you can use.\"",
  "\"Tokens turn up for people who keep at it.\"",
  "\"Don't thank me. Thank the street.\"",
  "\"Fewer of you every year. Not fewer tonight.\"",
  "\"Your Deck runs a little sharp. Mine did too, once.\"",
  "\"I know the hand that carried half these relics.\"",
  "\"Ask about the stall and I'll ask about your week.\"",
  "\"Prices are in pencil. They change.\"",
  "\"One street, one keeper. They hand it on.\"",
  "\"Paid in full. Your street can sleep.\"",
  "\"An early one. Haven't heard that wobble in years.\"",
  "\"Be polite to her. She was, to every caller.\"",
  "\"...\"",
  "\"He fixes bikes now. Still minds the curb.\"",
  "\"Which one? There have been six.\"",
  "\"Last train, every night. She never knew.\"",
  "\"That one's owner moved to the coast. Writes, sometimes.\"",
  "\"A night nurse's. Her ward got easier. Not her doing.\"",
  "\"A dog walker's. She moved owing. Her edge went heavy after.\"",
};

// Overheard: what a pulse or a still Deck sometimes catches out of the current,
// which is made of every message and call. Fiction, never real data. Each fits
// the footer (36 characters at most). A line's number is its place here, which
// the HEARD log keeps: add new lines at the end, and never reorder or remove one.
//
// Some lines are heard more where they belong (see App::overhear): near a kind
// of well, in a kind of ward, on a pulse or on the glass, or at wells you keep
// coming back to. A few are only ever heard there (pool 0). Never the clock.
enum : uint8_t { HW_Hollow = 1, HW_Fringe = 2, HW_Quarter = 4, HW_Nexus = 8 };   // wards (0: any)
enum : uint8_t { HowAny, HowPulse, HowGlass };
// HX_NotHome and HX_TithePage also hold the line back from the plain pool.
// HX_OldWell: the loudest well is one of the old radios on the lampposts.
enum : uint8_t { HX_Loan = 1, HX_LastVisit = 2, HX_Dawn = 4, HX_Keeper = 8, HX_NotHome = 16, HX_TithePage = 32,
                 HX_OldWell = 64 };
struct HeardBind {
  uint8_t wards, factions, energies;   // bit masks by enum order, 0 for any
  uint8_t how;                         // HowAny, HowPulse, HowGlass
  uint8_t extra;                       // HX_ flags, all must hold
  uint16_t vmin, vmax;                 // the well's sales across all runs
  uint8_t pool;                        // 1: also in the plain random pool
};
struct HeardLine { const char* text; HeardBind b; };
inline const HeardLine Overheard[] = {
  {"...said I'd call back...", {0, 0, 0, 0, 0, 0, 65535, 1}},
  {"...is the 14 still running?...", {4, 0, 0, 1, 0, 0, 65535, 1}},
  {"...no, the other door...", {6, 0, 0, 0, 0, 0, 65535, 1}},
  {"...tell her I'm sorry...", {0, 0, 0, 0, 0, 0, 65535, 1}},
  {"...two minutes, I'm parking...", {4, 0, 0, 0, 0, 0, 65535, 1}},
  {"...did you eat?...", {0, 0, 0, 0, 0, 0, 65535, 1}},
  {"...it's fine. It's fine...", {0, 0, 0, 0, 0, 0, 65535, 1}},
  {"...bring the charger...", {0, 7, 8, 2, 0, 0, 65535, 1}},
  {"...happy birthday, a day late...", {2, 2, 0, 0, 0, 0, 65535, 1}},
  {"...can you hear me now?...", {2, 2, 0, 0, 0, 0, 65535, 1}},
  {"...I miss the old place...", {2, 2, 0, 0, 0, 0, 65535, 1}},
  {"...left it on the counter...", {0, 2, 0, 0, 0, 0, 65535, 1}},
  {"...ok. ok. ok...", {0, 0, 0, 0, 0, 0, 65535, 1}},
  {"...who is this?...", {0, 0, 0, 0, 0, 0, 65535, 1}},
  {"...the lift's broken again...", {4, 0, 0, 0, 0, 0, 65535, 1}},
  {"...send me the photo...", {3, 0, 0, 1, 0, 0, 65535, 1}},
  {"...we should do this more...", {0, 0, 0, 0, 0, 10, 65535, 1}},
  {"...I'm outside...", {0, 0, 0, 0, 0, 0, 65535, 1}},
  {"...nobody tells me anything...", {2, 2, 0, 0, 0, 0, 65535, 1}},
  {"...it was raining there too...", {2, 2, 0, 0, 0, 0, 65535, 1}},
  {"...call your grandmother...", {2, 2, 0, 0, 0, 0, 65535, 1}},
  {"...see you Thursday...", {4, 0, 0, 1, 16, 0, 65535, 1}},
  {"...don't wait up...", {0, 0, 0, 0, 0, 0, 65535, 1}},
  {"...is anyone there?...", {0, 0, 8, 0, 16, 0, 65535, 1}},
  {"...that's not what I said...", {0, 0, 0, 0, 0, 0, 65535, 1}},
  {"...I'll walk, it's close...", {4, 0, 0, 1, 16, 0, 65535, 1}},
  {"...this block used to feel nicer...", {0, 0, 0, 0, 0, 0, 1, 1}},
  {"...nothing loads down here...", {12, 8, 0, 0, 0, 0, 65535, 1}},
  {"...they closed the corner cafe...", {2, 0, 0, 0, 0, 0, 65535, 1}},
  {"...my neighbor? Never met them...", {0, 0, 0, 0, 0, 0, 1, 1}},
  {"...chalk on the tile. You saw it?...", {0, 0, 0, 0, 0, 0, 65535, 1}},
  {"...the router just hums all night...", {0, 0, 8, 0, 16, 0, 65535, 1}},
  {"...take it to the office again...", {12, 8, 0, 0, 0, 0, 65535, 1}},
  {"...log it as Static. Nobody asks...", {12, 8, 0, 0, 0, 0, 65535, 1}},
  {"...nobody Warded sent them...", {0, 4, 0, 0, 0, 0, 65535, 1}},
  {"...it all goes upstairs...", {12, 8, 0, 0, 0, 0, 65535, 1}},
  {"...a friendly rate, he says...", {0, 8, 0, 0, 1, 0, 65535, 1}},
  {"...the building's full again...", {12, 8, 0, 0, 0, 0, 65535, 1}},
  {"...count it twice, then lock it...", {12, 8, 0, 0, 0, 0, 65535, 1}},
  {"...she says you sound thin...", {2, 2, 0, 0, 0, 0, 65535, 1}},
  {"...Gran, you're on the ceiling...", {2, 2, 0, 0, 0, 0, 65535, 1}},
  {"...she's sent socks again. Wool...", {2, 2, 0, 0, 0, 0, 65535, 1}},
  {"...same seat, same guy. Last bus...", {4, 0, 0, 1, 16, 0, 65535, 1}},
  {"...he signs his texts 73. Why?...", {4, 0, 0, 1, 16, 0, 65535, 1}},
  {"...73 means best regards. Radio...", {4, 0, 0, 1, 16, 0, 65535, 1}},
  {"...he fixed my earbuds with a pen...", {4, 0, 0, 1, 16, 0, 65535, 1}},
  {"...I'm happy here. I just miss it...", {2, 2, 0, 0, 0, 0, 65535, 1}},
  {"...is the Row all right? Really?...", {2, 2, 0, 0, 0, 0, 65535, 1}},
  {"...your mother grew up on the Row...", {2, 2, 0, 0, 0, 0, 65535, 1}},
  {"...the corner unit's still empty...", {2, 0, 0, 0, 0, 0, 65535, 1}},
  {"...she kept the good mugs, mind...", {2, 0, 0, 0, 0, 0, 65535, 1}},
  {"...she's at the market Saturdays...", {2, 0, 0, 0, 0, 0, 65535, 1}},
  {"...her buns sold out by ten, love...", {2, 0, 0, 0, 0, 0, 65535, 1}},
  {"...Gerald needs feeding by two...", {6, 0, 1, 0, 0, 0, 65535, 1}},
  {"...Gerald is a sourdough. Relax...", {6, 0, 1, 0, 0, 0, 65535, 1}},
  {"...first tray burnt. Second too...", {6, 0, 1, 0, 0, 0, 65535, 1}},
  {"...he says I knead like I'm cross...", {6, 0, 1, 0, 0, 0, 65535, 1}},
  {"...has anyone seen a grey dog?...", {3, 0, 0, 1, 0, 0, 65535, 1}},
  {"...answers to Biscuit. Sometimes...", {3, 0, 0, 1, 0, 0, 65535, 1}},
  {"...Dad, that's a fox. A FOX...", {3, 0, 0, 1, 0, 0, 65535, 1}},
  {"...his bowl's out on the step...", {3, 0, 0, 1, 0, 0, 65535, 1}},
  {"...he's not lost. He's exploring...", {3, 0, 0, 1, 0, 0, 65535, 1}},
  {"...he's at the bakery. The BAKERY...", {2, 0, 0, 1, 0, 0, 65535, 1}},
  {"...six croissants. He's had six...", {3, 0, 0, 1, 0, 0, 65535, 1}},
  {"...asleep on the sofa. Snoring...", {3, 0, 0, 1, 0, 0, 65535, 1}},
  {"...back at the bakery. Twice now...", {3, 0, 0, 1, 0, 0, 65535, 1}},
  {"...why's it called Static, then?...", {12, 8, 0, 0, 0, 0, 65535, 1}},
  {"...must be the heating. Must be...", {12, 8, 0, 0, 0, 0, 65535, 1}},
  {"...I stopped asking. It's easier...", {12, 8, 0, 0, 0, 0, 65535, 1}},
  {"...one more month and it's paid...", {12, 8, 0, 0, 1, 0, 65535, 1}},
  {"...it's ten minutes by a well...", {12, 8, 0, 0, 0, 0, 65535, 1}},
  {"...I used to keep a street. Mine...", {12, 8, 0, 0, 0, 0, 65535, 1}},
  {"...read the brick before you pour...", {0, 1, 0, 0, 0, 0, 65535, 1}},
  {"...knock twice first. Manners...", {0, 1, 0, 0, 0, 0, 65535, 1}},
  {"...guest ones count. They COUNT...", {0, 1, 0, 0, 0, 0, 65535, 1}},
  {"...chalk on brick, never doors...", {0, 1, 0, 0, 0, 0, 65535, 1}},
  {"...whole streets were open, once...", {0, 1, 0, 0, 0, 0, 65535, 1}},
  {"...people hurry past the steps...", {0, 0, 0, 0, 0, 0, 1, 1}},
  {"...thinking of moving, honestly...", {0, 0, 0, 0, 0, 0, 1, 1}},
  {"...it's the new bulbs, I think...", {0, 0, 0, 0, 0, 4, 9, 1}},
  {"...token at the last well. Always...", {0, 0, 0, 0, 2, 0, 65535, 1}},
  {"...arcade tokens don't COUNT...", {0, 0, 0, 0, 2, 0, 65535, 1}},
  {"...it's not for anyone. Manners...", {0, 0, 0, 0, 2, 0, 65535, 1}},
  {"...years of manners, and now?...", {0, 0, 0, 0, 2, 0, 65535, 1}},
  {"...he wants the crossword. Again...", {0, 7, 8, 2, 0, 0, 65535, 1}},
  {"...he did it in pen. Showing off...", {0, 7, 8, 2, 0, 0, 65535, 1}},
  {"...he says the night nurse hums...", {0, 7, 8, 2, 0, 0, 65535, 1}},
  {"...walked to the window himself...", {0, 7, 8, 2, 0, 0, 65535, 1}},
  {"...home Thursday. With a frame...", {0, 7, 8, 2, 0, 0, 65535, 1}},
  {"...he made the nurses a crossword...", {0, 7, 8, 2, 0, 0, 65535, 1}},
  {"...thank you for holding. Again...", {0, 0, 8, 0, 16, 0, 65535, 1}},
  {"...Grandad says nights get easier...", {0, 0, 8, 0, 16, 0, 65535, 1}},
  {"...he climbed poles forty years...", {0, 0, 8, 0, 16, 0, 65535, 1}},
  {"...swears he can hear a bad line...", {0, 0, 8, 0, 16, 0, 65535, 1}},
  {"...Gran's teaching next door...", {2, 2, 0, 0, 0, 0, 65535, 1}},
  {"...home Friday. She's made a list...", {2, 2, 0, 0, 0, 0, 65535, 1}},
  {"...missed my stop on purpose. Shh...", {4, 0, 0, 1, 16, 0, 65535, 1}},
  {"...he wrote 88. What's 88?...", {4, 0, 0, 1, 16, 0, 65535, 1}},
  {"...88 is love. I looked it up...", {4, 0, 0, 1, 16, 0, 65535, 1}},
  {"...I sent 88 back. Obviously...", {4, 0, 0, 1, 16, 0, 65535, 1}},
  {"...the day bus, together. Weird...", {4, 0, 0, 1, 16, 0, 65535, 1}},
  {"...she drove me down it Saturday...", {0, 0, 0, 0, 8, 0, 65535, 1}},
  {"...still feels right down there...", {0, 0, 0, 0, 8, 0, 65535, 1}},
  {"...he's down to Tuesdays only...", {6, 0, 1, 0, 0, 0, 65535, 1}},
  {"...he gave me Gerald. In a bucket...", {6, 0, 1, 0, 0, 0, 65535, 1}},
  {"...a grey dog's at the back door...", {2, 0, 0, 1, 0, 0, 65535, 1}},
  {"...my name's top of the rota...", {6, 0, 1, 0, 0, 0, 65535, 1}},
  {"...sold out by seven. SOLD OUT...", {6, 0, 1, 0, 0, 0, 65535, 1}},
  {"...he visits Gerald on Tuesdays...", {6, 0, 1, 0, 0, 0, 65535, 1}},
  {"...she poured quiet. I let it go...", {12, 8, 0, 0, 0, 0, 65535, 1}},
  {"...I never mentioned the chalk...", {12, 8, 0, 0, 0, 0, 65535, 1}},
  {"...handed the handheld in today...", {2, 2, 0, 0, 0, 0, 65535, 1}},
  {"...walked my old street. Thin...", {2, 2, 0, 0, 0, 0, 65535, 1}},
  {"...my street's coming back...", {2, 2, 0, 0, 0, 0, 65535, 1}},
  {"...chalked my first. It's wonky...", {0, 1, 0, 0, 0, 0, 65535, 1}},
  {"...she measured it. Goes nowhere...", {0, 0, 0, 0, 34, 0, 65535, 1}},
  {"...nowhere. I left one anyway...", {0, 0, 0, 0, 34, 0, 65535, 1}},
  {"...token's gone. Somebody took it...", {0, 0, 0, 0, 4, 0, 65535, 1}},
  {"...someone chalked 73 under it...", {0, 0, 0, 0, 4, 0, 65535, 1}},
  {"...see? It went somewhere...", {0, 0, 0, 0, 4, 0, 65535, 1}},
  {"...I said his line thing. Aloud...", {0, 0, 8, 0, 16, 0, 65535, 1}},
  {"...she runs a class now. Tuesdays...", {2, 2, 0, 0, 0, 5, 65535, 0}},
  {"...her whole street video calls...", {2, 2, 0, 0, 0, 5, 65535, 0}},
  {"...TO LET sign on the corner now...", {0, 0, 0, 0, 0, 10, 65535, 0}},
  {"...someone's measuring the corner...", {0, 0, 0, 0, 0, 10, 65535, 0}},
  {"...it's going to be soup. Soup!...", {0, 0, 0, 0, 0, 10, 65535, 0}},
  {"...she came to the opening. Buns...", {0, 0, 0, 0, 0, 10, 65535, 0}},
  {"...stairwell's nice again. Weird...", {0, 0, 0, 0, 0, 4, 9, 0}},
  {"...met the lady from upstairs...", {0, 0, 0, 0, 0, 4, 9, 0}},
  {"...kids are back on the steps...", {0, 0, 0, 0, 0, 10, 65535, 0}},
  {"...not moving. Changed my mind...", {0, 0, 0, 0, 0, 10, 65535, 0}},
  {"...same name, other side of town...", {0, 0, 0, 0, 0, 0, 65535, 1}},
  {"...two wells, one name. Lovers...", {0, 0, 0, 0, 0, 0, 65535, 1}},
  {"...it is one well. Has to be...", {0, 0, 0, 0, 0, 0, 65535, 1}},
  {"...the old pole radio's still on...", {0, 0, 0, 0, 64, 0, 65535, 0}},
};
constexpr int OverheardCount = sizeof(Overheard) / sizeof(Overheard[0]);

} // namespace Lore
