#pragma once
#include "Case.h"

// "The Terrace at Sai Krupa" - Sai Krupa CHS, Andheri West, Mumbai, during Navratri.
// Ramesh Kulkarni (62), society secretary, is found below the terrace parapet at 5:30am.
// Police call it an accident. TRUTH: Chairman Harpreet Singh Gill had secretly taken Rs. 1.2 crore from
// builder Mehta Realty through a shell firm ("GS Consultants") for pushing the redevelopment consent.
// Ramesh found out, asked Gill to meet him on the terrace at 9:30pm, and Gill pushed him at ~9:50pm
// while the garba was at its loudest, then padlocked the terrace and went back to the dhol.
inline Case makeMumbaiCase() {
    Case c("The Terrace at Sai Krupa", "compound", "s_gill");

    // ---------- Locations (7) ----------
    c.add(Location("compound", "Garba Ground", "Sai Krupa CHS, Andheri West. Marigold toran, a dhol stage, folding chairs and a taped-off patch of parking under the east wall.",
                   {"cabin", "society_office", "builder_office", "flat", "neighbour_flat", "terrace"}));
    c.add(Location("cabin", "Security Cabin", "Imran's cabin at the gate: a register, a key board, a small monitor and a steel tiffin.", {"compound"}));
    c.add(Location("society_office", "Society Office", "A cramped ground-floor room: steel almirahs, a rubber-stamp pad and framed AGM photos.", {"compound"}));
    c.add(Location("builder_office", "Mehta Realty Site Office", "A glass-door office across the lane with a scale model of 'Sai Krupa Towers'.",
                   {"compound"}, "c_visitor_book"));
    c.add(Location("flat", "Flat 701 (Kulkarni)", "A worn sofa, a carrom board, a Parle-G biscuit tin and a cup of cold tea.", {"compound"}));
    c.add(Location("neighbour_flat", "Flat 601 (Mrs. Iyer)", "Right below Ramesh's flat. Filter coffee, a kolam on the doorstep, a TV on mute.", {"compound"}));
    c.add(Location("terrace", "Terrace", "Water tanks, drying clothes and a knee-high parapet. The door is padlocked.", {"compound"}, "c_terrace_key"));

    // ---------- Clues (15) ----------
    // Garba ground
    c.add(Clue("c_fall_spot", "Where he fell", "Ramesh Kulkarni, 62, society secretary, was found below the east parapet at 5:30am. Police say accident: he checked the water tank every night. His phone is missing.", "compound"));
    c.add(Clue("c_bruise", "Dr. Reddy's observation", "A fresh finger-shaped bruise on his left forearm. 'Somebody gripped him before he fell,' says Dr. Reddy.", "compound", {"c_fall_spot"}));
    c.add(Clue("c_garba_video", "Garba videographer's footage", "Hiren's pendrive: the Chairman leads the garba at 9:15pm and again at 10:05pm. In between, the dhol stand he anchors sits empty for twenty minutes.",
               "compound", {"c_thud"}, "s_gill"));
    // Security cabin
    c.add(Clue("c_visitor_book", "Visitor register", "Mr. Mehta's car: in 7:50pm, out 8:40pm, 'met secretary'. Dr. Reddy's car: in 10:35pm.", "cabin"));
    c.add(Clue("c_terrace_key", "Spare terrace key", "The cabin key board holds a spare for the terrace padlock. Imran hands it over once he sees you are not blaming him.", "cabin", {"c_fall_spot"}));
    c.add(Clue("c_cctv_repair", "CCTV maintenance slip", "The staircase camera has been 'under maintenance' since 2 October. The work order is approved by the Chairman.", "cabin", {"c_terrace_key"}));
    // Society office
    c.add(Clue("c_rti", "RTI and EGM draft", "Ramesh drafted an EGM requisition and an RTI: 'What is GS Consultants, and why was it paid Rs. 1.2 crore?'", "society_office"));
    c.add(Clue("c_pooja_ledger", "Treasurer's ledger", "Pooja moved Rs. 2.4 lakh from the sinking fund for her son's fees and repaid it within a month. Ramesh knew and told her to disclose it.", "society_office"));
    c.add(Clue("c_key_register", "Key register", "The terrace padlock has three keys: the Secretary's, the Chairman's, and the spare on the cabin board.", "society_office", {"c_terrace_key"}));
    // Builder's office
    c.add(Clue("c_agreement", "Mehta Realty draft agreement", "A redevelopment 'consent bonus' of Rs. 1.2 crore payable to GS Consultants. Proprietor: H. S. Gill.", "builder_office", {"c_rti"}));
    // Flat 701
    c.add(Clue("c_diary", "Ramesh's diary", "'GS Consultants is Harpreet. Thirty years of carrom and he lied to my face. Will ask him to come to the terrace once the garba starts.'", "flat", {"c_rti"}));
    c.add(Clue("c_arjun_chat", "Arjun's WhatsApp chat", "Arjun pressed his father to sign the redevelopment consent. Ramesh: 'Not while I am alive.' Arjun's last text: 'Fine. Stuck on the expressway, will reach by eleven.'", "flat"));
    // Flat 601
    c.add(Clue("c_thud", "Mrs. Iyer's statement", "Two men's voices on the terrace around 9:40, then a thud at 9:50 on the loudest dhol beat. She thought it was a cracker. Then she heard the terrace padlock click.",
               "neighbour_flat", {"c_fall_spot"}));
    // Terrace
    c.add(Clue("c_phone", "Ramesh's phone", "Cracked, under the water tank. Last message, 9:28pm, to 'Harpreet Chairman': 'Terrace. Now. Bring the audit file.'",
               "terrace", {"c_diary"}, "s_gill"));
    c.add(Clue("c_padlock", "Terrace padlock", "Found locked from outside at 5:30am, yet Ramesh's own key was in his pocket. Only the Chairman's key could have locked it.", "terrace", {"c_key_register"}));

    // ---------- Suspects (7) ----------
    c.add(Suspect("s_gill", "Harpreet Singh Gill", "Society chairman, 63. Ramesh's carrom partner for thirty years.",
                  "I was leading the garba from eight to eleven, and I had not heard from Ramesh since the aarti."));
    c.add(Suspect("s_mehta", "Sanjay Mehta", "Builder, Mehta Realty. Wants the redevelopment approved.",
                  "I met Ramesh at eight, left by quarter to nine, and was at a builders' dinner in Vashi."));
    c.add(Suspect("s_pooja", "Pooja Sharma", "Society treasurer, 45.",
                  "I ran the prasad stall at the garba until half past ten."));
    c.add(Suspect("s_arjun", "Arjun Kulkarni", "Ramesh's son, 30. Wants to sell the flat.",
                  "I was driving back from Pune and reached home around eleven."));
    c.add(Suspect("s_iyer", "Meenakshi Iyer", "Retired teacher, 68, lives in 601.",
                  "I was watching TV in my flat and only heard crackers."));
    c.add(Suspect("s_imran", "Imran Sheikh", "Night security guard.",
                  "I was at the main gate managing parking all evening, and did my round at midnight."));
    c.add(Suspect("s_reddy", "Dr. Kavita Reddy", "Physician who lives in 702.",
                  "I was at my clinic until half past ten and saw nothing."));
    return c;
}
