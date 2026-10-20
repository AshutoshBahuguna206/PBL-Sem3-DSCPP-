#pragma once
#include "Case.h"

// "The Old Way" - Rastogi Kothi, Mussoorie. A landslide cuts the road at 11:40pm: the killer is still in the house.
//
// Dayanand Rastogi (76) is found dead in his library, door locked from inside. Everybody suspects the three
// heirs (he announced a new will at dinner). TRUTH: family lawyer Raghav Deshpande stole Rs. 2.3 crore from the
// family trust. Dayanand found out and summoned him to the library at eleven through a hidden passage built
// in 1921. Raghav slipped a sedative into the old man's milk and left the way he came, so the door stayed
// locked from inside. The player breaks the case by CROSS-EXAMINING people (catching lies with evidence) and by
// FORENSIC puzzles (body cooling, ink chromatography, a Samvat-to-year dial).
//
// Everyone has a job:
//   Dr. Reddy (witness)  gives the forensic facts and teaches the player how to cross-examine
//   Rohit, Meera, Kabir  each LIE to hide something embarrassing; exposing them gives the timeline
//   Shanta Bai           lies about the passage; exposing her opens the attic
//   Raghav               the killer: three incriminating lies
inline Case makeRastogiCase() {
    Case c("The Old Way", "hall", "s_raghav");

    // ---------- Locations (7) ----------
    c.add(Location("hall", "Grand Hall", "Rastogi Kothi, Mussoorie. Rain hammers the tin roof. A deodar staircase, last night's birthday table still laid. The road below collapsed: nobody has left, and nobody can.",
                   {"library", "kitchen", "family_wing", "blue_room", "attic"}));
    c.add(Location("library", "The Library", "Where Dayanand Rastogi died. Floor-to-ceiling teak shelves, a leather armchair, a fire still glowing.", {"hall", "passage"}));
    c.add(Location("kitchen", "Kitchen", "Shanta Bai's kitchen: steel patilas, a gas stove, the smell of cardamom and kesar.", {"hall"}));
    c.add(Location("family_wing", "Family Bedrooms", "Rohit's, Meera's and Kabir's rooms open onto one long corridor.", {"hall"}));
    c.add(Location("blue_room", "The Blue Room", "The guest room where Advocate Deshpande has stayed on every visit for twenty years. A big old wardrobe and a writing set on the dresser.", {"hall", "passage"}));
    c.add(Location("attic", "The Attic", "Dust, tin trunks, faded portraits. A cracked brass nameplate lies against a crate.", {"hall"}, "c_shanta_testimony"));
    c.add(Location("passage", "Hidden Passage and Strongroom", "A narrow stone passage thick with dust. It runs from the library to the Blue Room wardrobe, and ends at an iron door with a dial lock.",
                   {"library", "blue_room"}, "c_blueprint"));

    // ---------- Clues found in the rooms ----------
    c.add(Clue("c_dinner", "The birthday announcement", "At dinner Dayanand announced a new will, to be signed at 10am today. Rohit stormed out, Meera wept, Kabir laughed too loudly. At 11:40pm a landslide cut the road. The killer is still inside.", "hall"));
    c.add(Clue("c_scene", "The locked library", "Dayanand sat dead in this armchair. The door was locked from inside, the key in the lock, the windows bolted. Yet there are curved scrape marks in the dust by the third bookcase, as if something heavy swings open.", "library"));
    c.add(Clue("c_mill_report", "Mill report on the side table", "A paper-mill report in Rohit's handwriting, with a coffee ring and a note: 'R. 10:30 - the will'.", "library"));
    c.add(Clue("c_cheque", "Dayanand's cheque book", "His genuine signature, in royal-blue ink from his own fountain pen. A sample is ready for the lab.", "library"));
    c.add(Clue("c_milk_jug", "The kitchen milk jug", "The kesar milk left in the jug. Dr. Reddy's strip test is NEGATIVE. The sleeping drug went into the glass AFTER the milk left the kitchen.", "kitchen", {"c_milk_glass"}));
    c.add(Clue("c_betting", "Kabir's phone", "A betting app: Rs. 14 lakh lost on IPL. The last bet was placed at 11:05pm, using the verandah's wifi.", "family_wing"));
    c.add(Clue("c_blueprint", "The 1921 blueprint", "An architect's plan: a hidden passage joins the Library, the Blue Room wardrobe and a small strongroom. Only someone who knew it existed could use it.", "attic"));
    c.add(Clue("c_nameplate", "Cracked nameplate", "'Rastogi Kothi, Samvat 1978.' Vikram Samvat runs 57 years ahead of the English calendar.", "attic"));
    c.add(Clue("c_wife_text", "The lawyer's phone", "A text from Advocate Deshpande's wife: 'You hung up at 10:38 and never answered. Where did you go?' He told everyone the call lasted until 11:30.", "blue_room", {"c_kabir_testimony"}));
    c.add(Clue("c_pen", "Raghav's fountain pen", "A blue-black fountain pen from the writing set. A sample of its ink is ready for the lab.", "blue_room"));
    c.add(Clue("c_cufflink", "Gold cufflink", "A gold cufflink engraved 'R.D.', beside fresh footprints in the dust. The pair on Advocate Deshpande's dresser is missing one.", "passage"));
    c.add(Clue("c_strongroom_open", "The dial lock", "The dial wants a year in the English calendar. Samvat 1978 minus 57 is 1921. Click. The iron door swings open.", "passage", {"c_nameplate"}));
    c.add(Clue("c_forged_fd", "Forged certificates", "The family trust's fixed deposit certificates carry Dayanand's 'signature', but the originals show Rs. 2.3 crore siphoned over eleven years. A sample of the forged ink is ready for the lab.", "passage", {"c_strongroom_open"}));
    c.add(Clue("c_diary", "Dayanand's last diary entry", "'Called Raghav down the old way at eleven. Thirty years a friend. God, let him bring the money back.'", "passage", {"c_strongroom_open"}));
    c.add(Clue("c_new_will", "The real new will", "Everything in equal shares for Rohit, Meera and Kabir. The family had been fighting over a threat that never existed.", "passage", {"c_forged_fd"}));

    // ---------- Clues earned (confessions, the doctor's findings, lab results) ----------
    c.add(Clue("c_body", "Dr. Reddy: the body", "Body temperature at 7:15am: 29 C. The room was cold, about 17 C. A living body is 37 C.", "library").grantedOnly());
    c.add(Clue("c_milk_glass", "Dr. Reddy: the milk glass", "Dayanand's glass of kesar milk. Her strip test shows a sleeping drug that is NOT on his prescription.", "library").grantedOnly());
    c.add(Clue("c_rohit_testimony", "Rohit's confession", "Rohit visited at 10:30 to beg about the will. At 10:45 his father was alive and calm: 'Raghav comes at eleven. We will talk after he leaves.'", "library").grantedOnly());
    c.add(Clue("c_meera_testimony", "Meera's confession", "Meera carried the milk to the library at 10:50. Her father was alive, reading. The milk looked ordinary. He said: 'Raghav is coming at eleven. Leave us.'", "hall").grantedOnly());
    c.add(Clue("c_kabir_testimony", "Kabir's confession", "From the verandah at 11:05pm Kabir saw the Blue Room window DARK. Raghav says he sat in that room on a phone call.", "family_wing").grantedOnly());
    c.add(Clue("c_shanta_testimony", "Shanta Bai's confession", "The house does have a hidden passage. Sahab made her swear to keep it secret. She hands you the attic key. She also saw Meera carry the milk upstairs.", "kitchen").grantedOnly());
    c.add(Clue("c_tod", "Time of death", "Dayanand died around 11:15pm, shortly after his appointment with 'Raghav at eleven'.", "library").grantedOnly());
    c.add(Clue("c_ink_match", "Ink analysis result", "The forged signature's ink (Rf 0.71) matches RAGHAV's pen, not Dayanand's own (Rf 0.62) and not Rohit's (Rf 0.65).", "library").grantedOnly());

    // ---------- Forensic lab ----------
    c.addLab({"lab_tod", "Time of death (body cooling)", {"c_body"}, "c_tod"});
    c.addLab({"lab_ink", "Ink analysis (paper chromatography)", {"c_forged_fd", "c_cheque", "c_mill_report", "c_pen"}, "c_ink_match"});

    // ---------- People ----------
    c.add(Suspect("s_reddy", "Dr. Kavita Reddy", "Family physician. She examined the body and runs the forensic kit.", "I examined him at 7:15 this morning.").witness());
    c.add(Suspect("s_raghav", "Advocate Raghav Deshpande", "Family lawyer for thirty years. He called the police, calmed everyone, and has been the helpful one all morning.", "I was in the Blue Room all night."));
    c.add(Suspect("s_rohit", "Rohit Rastogi", "Elder son, runs the paper mills. Stern, secretive.", "I went to my room after dinner."));
    c.add(Suspect("s_meera", "Meera Bansal", "Daughter, estranged for years. Came home for the birthday.", "I was in my room."));
    c.add(Suspect("s_kabir", "Kabir Rastogi", "Grandson, 27, charming, always short of money.", "I was in bed."));
    c.add(Suspect("s_shanta", "Shanta Bai", "The cook, forty years in the house.", "I was in the kitchen."));

    // ---------- Statements (what people say; cross-examine them) ----------
    // Dr. Reddy: truthful. Pressing her gives forensic facts and teaches the rules.
    c.addStatement("s_reddy", {"I examined Dayanand at 7:15 this morning. His body temperature was 29 degrees. The room was cold, about 17.",
        "A living body is 37 degrees. After death it cools about one degree every hour. My lab kit is yours, Inspector: press L when you have something to test.", "c_body", {}, "", "", false});
    c.addStatement("s_reddy", {"His milk glass holds a sleeping drug. It is not on his prescription. I know every tablet he takes.",
        "My strip test is positive. Check whether the milk in the kitchen jug is clean.", "c_milk_glass", {}, "", "", false});
    c.addStatement("s_reddy", {"Whoever did this knew the house, and knew Dayanand's habits.",
        "Everyone here has a motive and a lie, Inspector. Cross-examine them: when someone says something false, press T and show them evidence that contradicts it. But be careful, a wrong accusation costs you credibility.", "", {}, "", "", false});

    // Raghav: the killer. Three incriminating lies.
    c.addStatement("s_raghav", {"I went up to the Blue Room at ten, spoke to my wife on the phone until half past eleven, and never left the room.",
        "Ask my wife, if you like. We spoke for ninety minutes.", "", {"c_wife_text"},
        "The call... dropped. Terrible signal in the hills. I stayed in my room. I... that must be a mistake.", "", true});
    c.addStatement("s_raghav", {"I have never been inside any hidden part of this house. Secret passages are servants' gossip.",
        "Dayanand's grandfather was a fanciful man. Nothing but rumours.", "", {"c_cufflink", "c_diary"},
        "A cufflink? Anyone could have dropped it there. And he wrote THAT about me? I... I need to sit down.", "", true});
    c.addStatement("s_raghav", {"The family trust is in perfect order. Every certificate was signed by Dayanand himself.",
        "Thirty years I have kept those books. Check them, if you doubt me.", "", {"c_ink_match", "c_forged_fd"},
        "He found out... Thirty years of loyalty, and one mistake. I only wanted to talk to him. I only meant to make him sleep, to take the papers back. He was not supposed to... He was never meant to die.", "", true});
    c.addStatement("s_raghav", {"Dayanand was my oldest friend. We played carrom every Sunday for thirty years.",
        "Last night I helped everyone. Somebody had to stay calm.", "", {}, "", "", false});

    // Rohit: lies to hide that he begged his father. Exposing him gives the timeline.
    c.addStatement("s_rohit", {"After dinner I went to my room and stayed there, with a sleeping pill, until morning.",
        "I sleep badly. A pill is the only way.", "", {"c_mill_report"},
        "Fine. I went to him at half past ten and begged him not to change the will. He was calm, and alive. He said Raghav was coming at eleven and we would talk after.", "c_rohit_testimony", false});
    c.addStatement("s_rohit", {"My father and I quarrelled about the will in front of everyone. But I never hurt him.",
        "I have hidden the mill's losses for two years. That is my shame. It is not a murder.", "", {}, "", "", false});

    // Meera: lies because she carried the milk and fears blame.
    c.addStatement("s_meera", {"I never went anywhere near my father's library last night. I was in my room, crying.",
        "I wanted to be alone.", "", {"c_shanta_testimony"},
        "Shanta saw me. Yes. I took his milk up at ten to eleven because I wanted to make peace. He was alive, reading. He said Raghav was coming at eleven and told me to leave.", "c_meera_testimony", false});
    c.addStatement("s_meera", {"My husband owes a crore. I came home to ask my father for help.",
        "He said no. I was angry. But I never wished him dead.", "", {}, "", "", false});

    // Kabir: lies to hide that he was gambling outside.
    c.addStatement("s_kabir", {"I was in bed by ten with a headache.",
        "Ask anyone. I never leave my room when I have a headache.", "", {"c_betting"},
        "Okay, okay! I was on the verandah, trying to win my money back. At five past eleven I looked up and the Blue Room window was dark. I did not think anything of it.", "c_kabir_testimony", false});
    c.addStatement("s_kabir", {"Babuji hated my betting. We were not close.",
        "But he was the only one who ever lent me money without a lecture.", "", {}, "", "", false});

    // Shanta Bai: lies about the passage (sworn to secrecy). Exposing her opens the attic.
    c.addStatement("s_shanta", {"There is no secret passage in Rastogi Kothi. It is an old servants' tale.",
        "Forty years I have worked here. I would know.", "", {"c_scene"},
        "...Sahab made us swear never to tell. Here, the attic key. And Inspector, I make the milk at ten, but it was Meera Bibi who carried it upstairs.", "c_shanta_testimony", false});
    c.addStatement("s_shanta", {"I make Sahab's kesar milk every night at ten. Forty years now.",
        "The jug stays in the kitchen. Nobody touches it but me.", "", {}, "", "", false});
    return c;
}
