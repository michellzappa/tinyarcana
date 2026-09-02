// The 22 majors, Rider-Waite-Smith order (Strength VIII, Justice XI).
// Elements and rulers follow the Golden Dawn attributions (Book T): twelve
// zodiac signs, seven planets, and the three mother-letter elements for the
// Fool, the Hanged Man and Judgement. glyphs.cpp draws the matching symbol. Rows are the
// three sevens of the Fool's Journey: I-VII the outer world, VIII-XIV the
// inner world, XV-XXI the greater world; the Fool stands on the threshold.
//
// Each position text is written to fit six lines of lora_body at 340 px.
#pragma once

#include <stdint.h>

enum Element : uint8_t { EL_FIRE = 0, EL_WATER, EL_AIR, EL_EARTH };

struct CardInfo {
  const char *name;
  const char *numeral;
  Element element;
  const char *ruler;
  const char *keywords;
  const char *essence;    // one line: what the card is
  const char *past;       // meaning in the Past position
  const char *present;
  const char *future;
  const char *question;   // closing question when it is the Future card
};

static const char *const ELEMENT_NAME[4] = {"Fire", "Water", "Air", "Earth"};
static const char *const POSITION_NAME[3] = {"Past", "Present", "Future"};

static const CardInfo CARDS[22] = {
  {"The Fool", "0", EL_AIR, "Air", "beginnings, trust, the leap",
   "The Fool is the step taken before the ground is proven.",
   "You started this without a map, on trust rather than proof. That innocence is what made the start possible. It also means some of what you carry was never examined.",
   "You stand at an edge, bag light, dog barking at your heels. The move in front of you cannot be made carefully. It can only be made.",
   "Something begins again, and it begins clean. Do not wait to feel ready. Readiness arrives after the step, never before it.",
   "What would you do if nothing had to be finished first?"},
  {"The Magician", "I", EL_AIR, "Mercury", "will, skill, focus",
   "The Magician has every tool on the table and knows the tools are not the point.",
   "You had what you needed and you used it. Skill and nerve got you here. Notice how much of the credit you have handed to luck instead.",
   "Everything required is already within reach. The work now is attention: one hand up, one hand down, and no wasted motion.",
   "You will be asked to act, not to prepare. When the moment comes, the tools will be the ones you have now.",
   "Which of your tools have you been pretending not to own?"},
  {"The High Priestess", "II", EL_WATER, "the Moon", "intuition, stillness, the unsaid",
   "The High Priestess keeps the scroll half hidden because the rest is not for words.",
   "Something was known and not spoken. You sensed the shape of this long before anyone named it. Trust that memory more than the record.",
   "Not everything here wants to be solved. Sit with what you already know. The answer is behind the veil, not in the questions you keep asking.",
   "Clarity will come from quiet, not from more information. Make room for it. What you cannot say yet will say itself in time.",
   "What do you already know that you have not let yourself hear?"},
  {"The Empress", "III", EL_EARTH, "Venus", "abundance, nurture, growth",
   "The Empress does not build. She grows, and lets the field do the work.",
   "You were held by something generous: a person, a place, a season of plenty. It shaped your sense of what is normal. Not everyone had that.",
   "This is a time to tend, not to force. What you feed will grow. So will what you neglect, in its own direction.",
   "Something ripens. It may take longer than you would like and arrive fuller than you expected. Be there to receive it.",
   "What are you feeding, and is it what you want more of?"},
  {"The Emperor", "IV", EL_FIRE, "Aries", "structure, authority, boundaries",
   "The Emperor sits still because the throne is the work.",
   "Order was imposed, by you or on you. It gave you a frame and it gave you walls. Both are still standing.",
   "Someone has to hold the line, and it is you. Set the terms plainly. A boundary said once, clearly, beats a rule enforced forever.",
   "You will be asked to lead, or to answer to someone who does. Decide now which kind of authority you can respect.",
   "Where does your structure protect you, and where does it just keep you seated?"},
  {"The Hierophant", "V", EL_EARTH, "Taurus", "tradition, teaching, belonging",
   "The Hierophant hands down what worked before and asks you to carry it.",
   "You learned the rules from someone who believed in them. Some of that teaching still fits. Some of it was theirs, not yours.",
   "You are inside an institution, a lineage or a habit older than you. Ask what it is for before you decide whether to keep it.",
   "A teacher, a group or a tradition is on the way. Belonging will be offered. Check the price on the way in, not the way out.",
   "Which inherited rule have you never actually tested?"},
  {"The Lovers", "VI", EL_AIR, "Gemini", "choice, union, values",
   "The Lovers is a choice that tells you who you are.",
   "A choice was made with the heart, and it set the course. Whether it was a person or a path, you chose it as yourself, not as a role.",
   "Two things you value are pulling apart. You will not get to keep both whole. Choose the one that makes you more yourself.",
   "A meeting or a decision ahead will ask for your whole self. Bring it. Half a yes is the one answer this does not accept.",
   "What are you choosing, and what are you choosing it over?"},
  {"The Chariot", "VII", EL_WATER, "Cancer", "drive, control, victory",
   "The Chariot moves by holding two pulls in one hand.",
   "You pushed through by will. It worked, and it cost you. Some of the tension you feel now is the reins you never put down.",
   "Momentum is on your side if you steer it. The two sphinxes want different roads. Your job is direction, not speed.",
   "You will get where you are going. The question the road will ask is whether you can arrive without the armour.",
   "What are you driving toward, and what is driving you?"},
  {"Strength", "VIII", EL_FIRE, "Leo", "patience, courage, gentleness",
   "Strength closes the lion's mouth with an open hand.",
   "You met something fierce with patience rather than force. That is why it did not devour you. Do not mistake that gentleness for weakness.",
   "The situation wants force. Give it steadiness instead. The lion is not your enemy; it is the part of you that is frightened.",
   "A test of temper is coming. You will pass it by staying soft where it expects you to be hard.",
   "What in you is roaring because nobody has calmed it?"},
  {"The Hermit", "IX", EL_EARTH, "Virgo", "solitude, search, guidance",
   "The Hermit climbs alone so the light can be seen from below.",
   "You withdrew, and you needed to. Something was found in that solitude that a crowd could not have given you.",
   "Step back. The noise is not going to answer this. Take the lamp and go where it is quiet enough to think.",
   "A period of retreat approaches, or a guide who has already walked this. Either way, the answer is up the mountain, not in the village.",
   "What are you hoping to find that you can only find alone?"},
  {"Wheel of Fortune", "X", EL_FIRE, "Jupiter", "turning, luck, cycles",
   "The Wheel turns whether or not you are holding on.",
   "Fortune moved and you moved with it. Some of what happened was never in your hands. Stop apportioning blame for the weather.",
   "You are mid-turn. What was up is coming down, and what was down is rising. Loosen your grip and watch for the pattern.",
   "A change of luck is ahead, and you will not see it coming. Position yourself where you would like to be when the wheel stops.",
   "Which cycle are you in, and how many times have you been here?"},
  {"Justice", "XI", EL_AIR, "Libra", "truth, balance, consequence",
   "Justice weighs what was done, not what was meant.",
   "Something was settled, fairly or not. A verdict was given. You are still living inside its terms.",
   "Take the situation exactly as it is, without flattering yourself. Actions have weight here. Balance the scales before someone else does.",
   "Consequence arrives. If you have been straight, it will feel like relief. If not, this is your notice.",
   "What would you decide if you had to be fair to everyone, including yourself?"},
  {"The Hanged Man", "XII", EL_WATER, "Water", "suspension, surrender, a new angle",
   "The Hanged Man is the only figure in the deck who is smiling.",
   "You were held in place and could not move things forward. Looking back, the pause taught you what forcing never could.",
   "Nothing is going to move by pushing. Stop pushing. Let yourself hang until the world turns the right way up.",
   "A delay is coming, and it is not a punishment. What you see upside down will be the true view.",
   "What would look different if you stopped fighting the position you are in?"},
  {"Death", "XIII", EL_WATER, "Scorpio", "ending, release, transformation",
   "Death clears the field so something else can be planted.",
   "Something ended, fully. There was no negotiating with it. Everything you are now grew in the space it left.",
   "An ending is here. Let it be complete. The grief is real, and the room it makes is real too.",
   "A closing is coming, and it will not be gentle about it. Do not cling. What survives will be what was truly yours.",
   "What are you keeping alive that has already finished?"},
  {"Temperance", "XIV", EL_FIRE, "Sagittarius", "blending, patience, the middle path",
   "Temperance pours between two cups until neither is empty.",
   "You found a middle way when the extremes were loud. Whatever it was, you mixed until it held.",
   "Nothing here needs to be all or nothing. Blend, adjust, test. The right measure is found by pouring slowly.",
   "Balance comes, but through practice, not arrival. Keep the two cups moving and you will not spill.",
   "Which two things in your life are waiting to be mixed?"},
  {"The Devil", "XV", EL_EARTH, "Capricorn", "bondage, appetite, the chain you hold",
   "The Devil's chains are loose. The figures have not noticed.",
   "Something held you that you agreed to. A habit, a bargain, a person. You called it necessary. Look at what it cost.",
   "You are bound, and the chain is slack. This is not about a villain. It is about the appetite you keep feeding.",
   "Temptation ahead, dressed as security. You will be able to walk away. Whether you do is the whole test.",
   "What are you calling a cage that you could simply put down?"},
  {"The Tower", "XVI", EL_FIRE, "Mars", "collapse, revelation, sudden change",
   "The Tower falls because it was built on the wrong thing.",
   "Something came down hard and fast. It felt like disaster. It was also the end of a structure that could not have held.",
   "The ground is moving. Do not rebuild yet. Let the false thing fall completely so you can see what was real underneath.",
   "A shock is coming, and it will save you time. What breaks was already broken. Hold on to what does not.",
   "What are you propping up that you would be freer without?"},
  {"The Star", "XVII", EL_AIR, "Aquarius", "hope, renewal, openness",
   "The Star pours water back into the pool with nothing to hide.",
   "After a hard time, something opened. Hope came back, quietly. That reservoir is still there when you need it.",
   "You are allowed to hope again. Be open about what you want. There is no armour on this figure and there need not be on you.",
   "Healing ahead, and a clearer sense of what you were made for. Trust it even if it arrives without proof.",
   "What do you want, if you say it without protecting yourself?"},
  {"The Moon", "XVIII", EL_WATER, "Pisces", "illusion, fear, the unclear path",
   "The Moon lights a path that looks different in the morning.",
   "You went through a time of not knowing what was real. Fear painted the shadows. Some of what you concluded then was the dark talking.",
   "Things are not as they appear. Do not make big decisions in this light. Note what frightens you and wait for the sun.",
   "Confusion ahead, or a truth that hides itself. Move slowly. The path continues even where you cannot see it.",
   "What are you afraid is true that you have not checked?"},
  {"The Sun", "XIX", EL_FIRE, "the Sun", "clarity, joy, vitality",
   "The Sun does not argue. It simply makes everything visible.",
   "There was a season of warmth and plain truth. You knew who you were. Return to that memory when the light goes thin.",
   "This is clear. Stop looking for the catch. Enjoy the thing in front of you with the whole of yourself.",
   "Success and warmth are coming, and they will be simple. Let them be simple. Do not complicate a good day.",
   "What is already good that you keep waiting to trust?"},
  {"Judgement", "XX", EL_FIRE, "Fire", "reckoning, awakening, the call",
   "Judgement is the trumpet you cannot pretend not to hear.",
   "You were called, and you answered. Something old was laid to rest and you rose from it changed.",
   "A reckoning is here. Look at the whole record, forgive what can be forgiven, and stand up. This is a summons, not a sentence.",
   "A call is coming that will ask you to rise to it. You will know it when you hear it. Do not make it repeat itself.",
   "What is calling you that you have been too busy to answer?"},
  {"The World", "XXI", EL_EARTH, "Saturn", "completion, wholeness, arrival",
   "The World is the dance at the end that turns out to be the beginning.",
   "Something was completed, and completed well. You closed a circle. What you learned there is the ground you stand on.",
   "You are nearly whole. Finish it. The last step is the one that makes all the others count.",
   "Arrival. A cycle closes with everything in its place. Then the dance starts again, one level up.",
   "What would it mean to be finished, and what would you start next?"},
};

static inline const char *cardPositionText(uint8_t idx, uint8_t pos) {
  const CardInfo &c = CARDS[idx];
  return pos == 0 ? c.past : pos == 1 ? c.present : c.future;
}
