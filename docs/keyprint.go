package main

import (
  "text/template"
  "os"
  "errors"
  "bytes"
  "strings"
  "time"
  "fmt"
  "flag"
  "unicode"
  "bufio"

  "crypto/sha256"
  "encoding/hex"
  "encoding/binary"
  "io"
  "math/big"

  "crypto/rand"
  "hash/crc32"

  "github.com/mdp/qrterminal/v3"

  "golang.org/x/crypto/sha3"
  "golang.org/x/crypto/ripemd160"

  "filippo.io/edwards25519"

  "github.com/tyler-smith/go-bip32"
  "github.com/tyler-smith/go-bip39"
  "github.com/FactomProject/basen"
  btcutil "github.com/FactomProject/btcutilecc"
  "github.com/bytom/bytom/common/bech32"
)

var (
	curve       = btcutil.Secp256k1()
	curveParams = curve.Params()

	// BitcoinBase58Encoding is the encoding used for bitcoin addresses
	BitcoinBase58Encoding = basen.NewEncoding("123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz")

  C330CharSet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-/.',& "

  // https://github.com/monero-project/monero/blob/master/src/mnemonics/english.h
  XMRWordsEnglish = []string{"abbey","abducts","ability","ablaze","abnormal","abort","abrasive","absorb","abyss","academy","aces","aching","acidic","acoustic","acquire","across","actress","acumen","adapt","addicted","adept","adhesive","adjust","adopt","adrenalin","adult","adventure","aerial","afar","affair","afield","afloat","afoot","afraid","after","against","agenda","aggravate","agile","aglow","agnostic","agony","agreed","ahead","aided","ailments","aimless","airport","aisle","ajar","akin","alarms","album","alchemy","alerts","algebra","alkaline","alley","almost","aloof","alpine","already","also","altitude","alumni","always","amaze","ambush","amended","amidst","ammo","amnesty","among","amply","amused","anchor","android","anecdote","angled","ankle","annoyed","answers","antics","anvil","anxiety","anybody","apart","apex","aphid","aplomb","apology","apply","apricot","aptitude","aquarium","arbitrary","archer","ardent","arena","argue","arises","army","around","arrow","arsenic","artistic","ascend","ashtray","aside","asked","asleep","aspire","assorted","asylum","athlete","atlas","atom","atrium","attire","auburn","auctions","audio","august","aunt","austere","autumn","avatar","avidly","avoid","awakened","awesome","awful","awkward","awning","awoken","axes","axis","axle","aztec","azure","baby","bacon","badge","baffles","bagpipe","bailed","bakery","balding","bamboo","banjo","baptism","basin","batch","bawled","bays","because","beer","befit","begun","behind","being","below","bemused","benches","berries","bested","betting","bevel","beware","beyond","bias","bicycle","bids","bifocals","biggest","bikini","bimonthly","binocular","biology","biplane","birth","biscuit","bite","biweekly","blender","blip","bluntly","boat","bobsled","bodies","bogeys","boil","boldly","bomb","border","boss","both","bounced","bovine","bowling","boxes","boyfriend","broken","brunt","bubble","buckets","budget","buffet","bugs","building","bulb","bumper","bunch","business","butter","buying","buzzer","bygones","byline","bypass","cabin","cactus","cadets","cafe","cage","cajun","cake","calamity","camp","candy","casket","catch","cause","cavernous","cease","cedar","ceiling","cell","cement","cent","certain","chlorine","chrome","cider","cigar","cinema","circle","cistern","citadel","civilian","claim","click","clue","coal","cobra","cocoa","code","coexist","coffee","cogs","cohesive","coils","colony","comb","cool","copy","corrode","costume","cottage","cousin","cowl","criminal","cube","cucumber","cuddled","cuffs","cuisine","cunning","cupcake","custom","cycling","cylinder","cynical","dabbing","dads","daft","dagger","daily","damp","dangerous","dapper","darted","dash","dating","dauntless","dawn","daytime","dazed","debut","decay","dedicated","deepest","deftly","degrees","dehydrate","deity","dejected","delayed","demonstrate","dented","deodorant","depth","desk","devoid","dewdrop","dexterity","dialect","dice","diet","different","digit","dilute","dime","dinner","diode","diplomat","directed","distance","ditch","divers","dizzy","doctor","dodge","does","dogs","doing","dolphin","domestic","donuts","doorway","dormant","dosage","dotted","double","dove","down","dozen","dreams","drinks","drowning","drunk","drying","dual","dubbed","duckling","dude","duets","duke","dullness","dummy","dunes","duplex","duration","dusted","duties","dwarf","dwelt","dwindling","dying","dynamite","dyslexic","each","eagle","earth","easy","eating","eavesdrop","eccentric","echo","eclipse","economics","ecstatic","eden","edgy","edited","educated","eels","efficient","eggs","egotistic","eight","either","eject","elapse","elbow","eldest","eleven","elite","elope","else","eluded","emails","ember","emerge","emit","emotion","empty","emulate","energy","enforce","enhanced","enigma","enjoy","enlist","enmity","enough","enraged","ensign","entrance","envy","epoxy","equip","erase","erected","erosion","error","eskimos","espionage","essential","estate","etched","eternal","ethics","etiquette","evaluate","evenings","evicted","evolved","examine","excess","exhale","exit","exotic","exquisite","extra","exult","fabrics","factual","fading","fainted","faked","fall","family","fancy","farming","fatal","faulty","fawns","faxed","fazed","feast","february","federal","feel","feline","females","fences","ferry","festival","fetches","fever","fewest","fiat","fibula","fictional","fidget","fierce","fifteen","fight","films","firm","fishing","fitting","five","fixate","fizzle","fleet","flippant","flying","foamy","focus","foes","foggy","foiled","folding","fonts","foolish","fossil","fountain","fowls","foxes","foyer","framed","friendly","frown","fruit","frying","fudge","fuel","fugitive","fully","fuming","fungal","furnished","fuselage","future","fuzzy","gables","gadget","gags","gained","galaxy","gambit","gang","gasp","gather","gauze","gave","gawk","gaze","gearbox","gecko","geek","gels","gemstone","general","geometry","germs","gesture","getting","geyser","ghetto","ghost","giant","giddy","gifts","gigantic","gills","gimmick","ginger","girth","giving","glass","gleeful","glide","gnaw","gnome","goat","goblet","godfather","goes","goggles","going","goldfish","gone","goodbye","gopher","gorilla","gossip","gotten","gourmet","governing","gown","greater","grunt","guarded","guest","guide","gulp","gumball","guru","gusts","gutter","guys","gymnast","gypsy","gyrate","habitat","hacksaw","haggled","hairy","hamburger","happens","hashing","hatchet","haunted","having","hawk","haystack","hazard","hectare","hedgehog","heels","hefty","height","hemlock","hence","heron","hesitate","hexagon","hickory","hiding","highway","hijack","hiker","hills","himself","hinder","hippo","hire","history","hitched","hive","hoax","hobby","hockey","hoisting","hold","honked","hookup","hope","hornet","hospital","hotel","hounded","hover","howls","hubcaps","huddle","huge","hull","humid","hunter","hurried","husband","huts","hybrid","hydrogen","hyper","iceberg","icing","icon","identity","idiom","idled","idols","igloo","ignore","iguana","illness","imagine","imbalance","imitate","impel","inactive","inbound","incur","industrial","inexact","inflamed","ingested","initiate","injury","inkling","inline","inmate","innocent","inorganic","input","inquest","inroads","insult","intended","inundate","invoke","inwardly","ionic","irate","iris","irony","irritate","island","isolated","issued","italics","itches","items","itinerary","itself","ivory","jabbed","jackets","jaded","jagged","jailed","jamming","january","jargon","jaunt","javelin","jaws","jazz","jeans","jeers","jellyfish","jeopardy","jerseys","jester","jetting","jewels","jigsaw","jingle","jittery","jive","jobs","jockey","jogger","joining","joking","jolted","jostle","journal","joyous","jubilee","judge","juggled","juicy","jukebox","july","jump","junk","jury","justice","juvenile","kangaroo","karate","keep","kennel","kept","kernels","kettle","keyboard","kickoff","kidneys","king","kiosk","kisses","kitchens","kiwi","knapsack","knee","knife","knowledge","knuckle","koala","laboratory","ladder","lagoon","lair","lakes","lamb","language","laptop","large","last","later","launching","lava","lawsuit","layout","lazy","lectures","ledge","leech","left","legion","leisure","lemon","lending","leopard","lesson","lettuce","lexicon","liar","library","licks","lids","lied","lifestyle","light","likewise","lilac","limits","linen","lion","lipstick","liquid","listen","lively","loaded","lobster","locker","lodge","lofty","logic","loincloth","long","looking","lopped","lordship","losing","lottery","loudly","love","lower","loyal","lucky","luggage","lukewarm","lullaby","lumber","lunar","lurk","lush","luxury","lymph","lynx","lyrics","macro","madness","magically","mailed","major","makeup","malady","mammal","maps","masterful","match","maul","maverick","maximum","mayor","maze","meant","mechanic","medicate","meeting","megabyte","melting","memoir","menu","merger","mesh","metro","mews","mice","midst","mighty","mime","mirror","misery","mittens","mixture","moat","mobile","mocked","mohawk","moisture","molten","moment","money","moon","mops","morsel","mostly","motherly","mouth","movement","mowing","much","muddy","muffin","mugged","mullet","mumble","mundane","muppet","mural","musical","muzzle","myriad","mystery","myth","nabbing","nagged","nail","names","nanny","napkin","narrate","nasty","natural","nautical","navy","nearby","necklace","needed","negative","neither","neon","nephew","nerves","nestle","network","neutral","never","newt","nexus","nibs","niche","niece","nifty","nightly","nimbly","nineteen","nirvana","nitrogen","nobody","nocturnal","nodes","noises","nomad","noodles","northern","nostril","noted","nouns","novelty","nowhere","nozzle","nuance","nucleus","nudged","nugget","nuisance","null","number","nuns","nurse","nutshell","nylon","oaks","oars","oasis","oatmeal","obedient","object","obliged","obnoxious","observant","obtains","obvious","occur","ocean","october","odds","odometer","offend","often","oilfield","ointment","okay","older","olive","olympics","omega","omission","omnibus","onboard","oncoming","oneself","ongoing","onion","online","onslaught","onto","onward","oozed","opacity","opened","opposite","optical","opus","orange","orbit","orchid","orders","organs","origin","ornament","orphans","oscar","ostrich","otherwise","otter","ouch","ought","ounce","ourselves","oust","outbreak","oval","oven","owed","owls","owner","oxidant","oxygen","oyster","ozone","pact","paddles","pager","pairing","palace","pamphlet","pancakes","paper","paradise","pastry","patio","pause","pavements","pawnshop","payment","peaches","pebbles","peculiar","pedantic","peeled","pegs","pelican","pencil","people","pepper","perfect","pests","petals","phase","pheasants","phone","phrases","physics","piano","picked","pierce","pigment","piloted","pimple","pinched","pioneer","pipeline","pirate","pistons","pitched","pivot","pixels","pizza","playful","pledge","pliers","plotting","plus","plywood","poaching","pockets","podcast","poetry","point","poker","polar","ponies","pool","popular","portents","possible","potato","pouch","poverty","powder","pram","present","pride","problems","pruned","prying","psychic","public","puck","puddle","puffin","pulp","pumpkins","punch","puppy","purged","push","putty","puzzled","pylons","pyramid","python","queen","quick","quote","rabbits","racetrack","radar","rafts","rage","railway","raking","rally","ramped","randomly","rapid","rarest","rash","rated","ravine","rays","razor","react","rebel","recipe","reduce","reef","refer","regular","reheat","reinvest","rejoices","rekindle","relic","remedy","renting","reorder","repent","request","reruns","rest","return","reunion","revamp","rewind","rhino","rhythm","ribbon","richly","ridges","rift","rigid","rims","ringing","riots","ripped","rising","ritual","river","roared","robot","rockets","rodent","rogue","roles","romance","roomy","roped","roster","rotate","rounded","rover","rowboat","royal","ruby","rudely","ruffled","rugged","ruined","ruling","rumble","runway","rural","rustled","ruthless","sabotage","sack","sadness","safety","saga","sailor","sake","salads","sample","sanity","sapling","sarcasm","sash","satin","saucepan","saved","sawmill","saxophone","sayings","scamper","scenic","school","science","scoop","scrub","scuba","seasons","second","sedan","seeded","segments","seismic","selfish","semifinal","sensible","september","sequence","serving","session","setup","seventh","sewage","shackles","shelter","shipped","shocking","shrugged","shuffled","shyness","siblings","sickness","sidekick","sieve","sifting","sighting","silk","simplest","sincerely","sipped","siren","situated","sixteen","sizes","skater","skew","skirting","skulls","skydive","slackens","sleepless","slid","slower","slug","smash","smelting","smidgen","smog","smuggled","snake","sneeze","sniff","snout","snug","soapy","sober","soccer","soda","software","soggy","soil","solved","somewhere","sonic","soothe","soprano","sorry","southern","sovereign","sowed","soya","space","speedy","sphere","spiders","splendid","spout","sprig","spud","spying","square","stacking","stellar","stick","stockpile","strained","stunning","stylishly","subtly","succeed","suddenly","suede","suffice","sugar","suitcase","sulking","summon","sunken","superior","surfer","sushi","suture","swagger","swept","swiftly","sword","swung","syllabus","symptoms","syndrome","syringe","system","taboo","tacit","tadpoles","tagged","tail","taken","talent","tamper","tanks","tapestry","tarnished","tasked","tattoo","taunts","tavern","tawny","taxi","teardrop","technical","tedious","teeming","tell","template","tender","tepid","tequila","terminal","testing","tether","textbook","thaw","theatrics","thirsty","thorn","threaten","thumbs","thwart","ticket","tidy","tiers","tiger","tilt","timber","tinted","tipsy","tirade","tissue","titans","toaster","tobacco","today","toenail","toffee","together","toilet","token","tolerant","tomorrow","tonic","toolbox","topic","torch","tossed","total","touchy","towel","toxic","toyed","trash","trendy","tribal","trolling","truth","trying","tsunami","tubes","tucks","tudor","tuesday","tufts","tugs","tuition","tulips","tumbling","tunnel","turnip","tusks","tutor","tuxedo","twang","tweezers","twice","twofold","tycoon","typist","tyrant","ugly","ulcers","ultimate","umbrella","umpire","unafraid","unbending","uncle","under","uneven","unfit","ungainly","unhappy","union","unjustly","unknown","unlikely","unmask","unnoticed","unopened","unplugs","unquoted","unrest","unsafe","until","unusual","unveil","unwind","unzip","upbeat","upcoming","update","upgrade","uphill","upkeep","upload","upon","upper","upright","upstairs","uptight","upwards","urban","urchins","urgent","usage","useful","usher","using","usual","utensils","utility","utmost","utopia","uttered","vacation","vague","vain","value","vampire","vane","vapidly","vary","vastness","vats","vaults","vector","veered","vegan","vehicle","vein","velvet","venomous","verification","vessel","veteran","vexed","vials","vibrate","victim","video","viewpoint","vigilant","viking","village","vinegar","violin","vipers","virtual","visited","vitals","vivid","vixen","vocal","vogue","voice","volcano","vortex","voted","voucher","vowels","voyage","vulture","wade","waffle","wagtail","waist","waking","wallets","wanted","warped","washing","water","waveform","waxing","wayside","weavers","website","wedge","weekday","weird","welders","went","wept","were","western","wetsuit","whale","when","whipped","whole","wickets","width","wield","wife","wiggle","wildly","winter","wipeout","wiring","wise","withdrawn","wives","wizard","wobbly","woes","woken","wolf","womanly","wonders","woozy","worry","wounded","woven","wrap","wrist","wrong","yacht","yahoo","yanks","yard","yawning","yearbook","yellow","yesterday","yeti","yields","yodel","yoga","younger","yoyo","zapped","zeal","zebra","zero","zesty","zigzags","zinger","zippers","zodiac","zombie","zones","zoom"}
)

type PKey_BIP39 struct {
  Mnemonic [24]string
}

type PKey_XMR_Mnemonic struct {
  Mnemonic [25]string
}

const PKey_Info_Print_Template = `
<]F0 SY540SX860
Y050X100 CI10
Y125X100 CI10
Y200X100 CI10
Y275X100 CI10
Y350X100 CI10
Y425X100 CI10>

<POLVI HD WALLET
24 WORD BIP32 MNEMONIC
BTC BIP84 PATH
M/84'/0'/0'/0/0
ETH BIP44 PATH
M/44'/60'/0'/0/0>
`
const PKey_XMR_Info_Print_Template = `
<]F0 SY540SX860
Y125X100 CI10
Y200X100 CI10
Y275X100 CI10
Y350X100 CI10>

<POLVI HD WALLET
MONERO
25 WORD MNEMONIC SEED
ACCOUNT NUMBER 0>
`

const PKey_BIP39_Print_Template0 = `
<]F1 SY540SX860
Y050X100 CI10
Y125X100 CI10
Y200X100 CI10
Y275X100 CI10
Y350X100 CI10
Y425X100 CI10>

<{{printf "%2d" 1}} {{printf "%-11s" (index .Mnemonic 0) -}}
{{ printf "%2d" 2 }} {{index .Mnemonic 1}}
{{printf "%2d" 3}} {{printf "%-11s" (index .Mnemonic 2) -}}
{{ printf "%2d" 4 }} {{index .Mnemonic 3}}
{{printf "%2d" 5}} {{printf "%-11s" (index .Mnemonic 4) -}}
{{ printf "%2d" 6 }} {{index .Mnemonic 5}}
{{printf "%2d" 7}} {{printf "%-11s" (index .Mnemonic 6) -}}
{{ printf "%2d" 8 }} {{index .Mnemonic 7}}
{{printf "%2d" 9}} {{printf "%-11s" (index .Mnemonic 8) -}}
{{ printf "%2d" 10 }} {{index .Mnemonic 9}}
{{printf "%2d" 11}} {{printf "%-11s" (index .Mnemonic 10) -}}
{{ printf "%2d" 12 }} {{index .Mnemonic 11}}>
`

const PKey_BIP39_Print_Template1 = `
<]F2 SY540SX860
Y050X100 CI10
Y125X100 CI10
Y200X100 CI10
Y275X100 CI10
Y350X100 CI10
Y425X100 CI10>

<{{printf "%2d" 13}} {{printf "%-11s" (index .Mnemonic 12) -}}
{{ printf "%2d" 14 }} {{index .Mnemonic 13}}
{{printf "%2d" 15}} {{printf "%-11s" (index .Mnemonic 14) -}}
{{ printf "%2d" 16 }} {{index .Mnemonic 15}}
{{printf "%2d" 17}} {{printf "%-11s" (index .Mnemonic 16) -}}
{{ printf "%2d" 18 }} {{index .Mnemonic 17}}
{{printf "%2d" 19}} {{printf "%-11s" (index .Mnemonic 18) -}}
{{ printf "%2d" 20 }} {{index .Mnemonic 19}}
{{printf "%2d" 21}} {{printf "%-11s" (index .Mnemonic 20) -}}
{{ printf "%2d" 22 }} {{index .Mnemonic 21}}
{{printf "%2d" 23}} {{printf "%-11s" (index .Mnemonic 22) -}}
{{ printf "%2d" 24 }} {{index .Mnemonic 23}}>
`

const PKey_XMR_Mnemonic_Print_Template0 = `
<]F1 SY540SX860
Y085X100 CI10
Y160X100 CI10
Y235X100 CI10
Y310X100 CI10
Y385X100 CI10>

<{{printf "%2d" 1}} {{printf "%-12s" (index .Mnemonic 0) -}}
{{ printf "%2d" 2 }} {{index .Mnemonic 1}}
{{printf "%2d" 3}} {{printf "%-12s" (index .Mnemonic 2) -}}
{{ printf "%2d" 4 }} {{index .Mnemonic 3}}
{{printf "%2d" 5}} {{printf "%-12s" (index .Mnemonic 4) -}}
{{ printf "%2d" 6 }} {{index .Mnemonic 5}}
{{printf "%2d" 7}} {{printf "%-12s" (index .Mnemonic 6) -}}
{{ printf "%2d" 8 }} {{index .Mnemonic 7}}
{{printf "%2d" 9}} {{printf "%-12s" (index .Mnemonic 8) -}}
{{ printf "%2d" 10 }} {{index .Mnemonic 9}}>
`

const PKey_XMR_Mnemonic_Print_Template1 = `
<]F1 SY540SX860
Y085X100 CI10
Y160X100 CI10
Y235X100 CI10
Y310X100 CI10
Y385X100 CI10>

<{{printf "%2d" 11}} {{printf "%-12s" (index .Mnemonic 10) -}}
{{ printf "%2d" 12 }} {{index .Mnemonic 11}}
{{printf "%2d" 13}} {{printf "%-12s" (index .Mnemonic 12) -}}
{{ printf "%2d" 14 }} {{index .Mnemonic 13}}
{{printf "%2d" 15}} {{printf "%-12s" (index .Mnemonic 14) -}}
{{ printf "%2d" 16 }} {{index .Mnemonic 15}}
{{printf "%2d" 17}} {{printf "%-12s" (index .Mnemonic 16) -}}
{{ printf "%2d" 18 }} {{index .Mnemonic 17}}
{{printf "%2d" 19}} {{printf "%-12s" (index .Mnemonic 18) -}}
{{ printf "%2d" 20 }} {{index .Mnemonic 19}}>
`

const PKey_XMR_Mnemonic_Print_Template2 = `
<]F1 SY540SX860
Y085X100 CI10
Y160X100 CI10
Y235X100 CI10
Y310X100 CI10
Y385X100 CI10>

<{{printf "%2d" 21}} {{printf "%-12s" (index .Mnemonic 20) }}
{{printf "%2d" 22}} {{printf "%-12s" (index .Mnemonic 21) }}
{{printf "%2d" 23}} {{printf "%-12s" (index .Mnemonic 22) }}
{{printf "%2d" 24}} {{printf "%-12s" (index .Mnemonic 23) }}
{{printf "%2d" 25}} {{printf "%-12s" (index .Mnemonic 24) }}>
`

type PKey struct {
  MNTDate string
  BTCAddr string
  ETHAddr string
  ETHHeader string
  BTCHeader string
  Layout string
  MNTMessage string
  Message string
  Message2 string
  Message3 string
  Message4 string
  Message5 string
}

type PKey_XMR struct {
  MNTDate string
  Addr string
  Header string
  Layout string
  MNTMessage string
  Message string
  Message2 string
  Message3 string
  Message4 string
  Message5 string
}

const PKey_BTCAddr_Message2_Template = `{{.BTCHeader}} {{printf "%.19s" .BTCAddr}}...`
const PKey_BTCAddr_Message3_Template = `...{{printf "%s" (slice .BTCAddr 19)}}`
const PKey_BTCAddr_Message4_Template = PKey_BTCAddr_Message2_Template
const PKey_BTCAddr_Message5_Template = PKey_BTCAddr_Message3_Template
const PKey_ETHAddr_Message2_Template = `{{.ETHHeader}} {{printf "%.19s" .ETHAddr}}...`
const PKey_ETHAddr_Message3_Template = `...{{printf "%s" (slice .ETHAddr 19)}}`
const PKey_ETHAddr_Message4_Template = PKey_ETHAddr_Message2_Template
const PKey_ETHAddr_Message5_Template = PKey_ETHAddr_Message3_Template

const PKey_MNTDate_Template = `MINTED ON {{.MNTDate}}`

const PKey_XMR_Addr_Message2_Template = `{{.Header}}{{printf "%.22s" .Addr}}-`
const PKey_XMR_Addr_Message3_Template = `-{{printf "%.24s" (slice .Addr 22)}}-`
const PKey_XMR_Addr_Message4_Template = `-{{printf "%.24s" (slice .Addr 46)}}-`
const PKey_XMR_Addr_Message5_Template = `-{{printf "%.25s" (slice .Addr 70)}}`

func PKey_Layout(pkey PKey) (string) {
  ly := []string{}
  y := 50
  x := 100
  ly = append(ly, MX_Layout(pkey.MNTMessage, y, x)) // MNTDate
  y += 75
  ly = append(ly, MX_Layout(pkey.Message, y, x)) // Message
  y += 75
  ly = append(ly, MX_Layout(pkey.Message2, y, x)) // Message 2 || ETHAddr || BTCAddr
  y += 75
  ly = append(ly, MX_Layout(pkey.Message3, y, x)) // Message 3 || ETHAddr || BTCAddr
  y += 75
  ly = append(ly, MX_Layout(pkey.Message4, y, x)) // Message 4 || EthAddr || BTCAddr
  y += 75
  ly = append(ly, MX_Layout(pkey.Message5, y, x)) // Message 5 || EthAddr || BTCAddr
  return strings.Join(ly, "\n")
}

func PKey_XMR_Layout(pkey PKey_XMR) (string) {
  ly := []string{}
  y := 50
  x := 100
  ly = append(ly, MX_Layout(pkey.MNTMessage, y, x)) // MNTDate
  y += 75
  ly = append(ly, MX_Layout(pkey.Message, y, x)) // Message
  y += 75
  ly = append(ly, MX_Layout(pkey.Message2, y, x)) // Message 2 || ETHAddr || BTCAddr
  y += 75
  ly = append(ly, MX_Layout(pkey.Message3, y, x)) // Message 3 || ETHAddr || BTCAddr
  y += 75
  ly = append(ly, MX_Layout(pkey.Message4, y, x)) // Message 4 || EthAddr || BTCAddr
  y += 75
  ly = append(ly, MX_Layout(pkey.Message5, y, x)) // Message 5 || EthAddr || BTCAddr
  return strings.Join(ly, "\n")
}


func YX_Layout(y int, x int) (string) {
  return fmt.Sprintf("Y%03dX%03d CI10", y, x)
}

func MX_Layout(s string, y int, x int) string {
  if strings.ToUpper(s) == string(s) {
    return YX_Layout(y, x)
  }
  return YX_Layout(y, x) + "\n" + YX_Layout(y+7, x)
}

func MX_Message(s string) string {
  if strings.ToUpper(s) == string(s) {
    return s
  }
  su := []rune{}
  sl := []rune{}
  for _, r := range(s) {
    if strings.ToUpper(string(r)) == string(r) {
      su = append(su, r)
      sl = append(sl, ' ')
    } else {
      su = append(su, ' ')
      sl = append(sl, r)
    }
  }
  return string(su) + "\n" + string(sl)
}

func KeyPrint_PKey(pkey PKey, printbtc, printeth bool) (string, error) {
  tmpl, err := template.New("keyprint").Parse(PKey_MNTDate_Template)
  if err != nil { return "", err }
  buf := new(bytes.Buffer)
  err = tmpl.Execute(buf, pkey)
  pkey.MNTMessage = MX_Message(buf.String())
  if printbtc && printeth {
    // No extra messages: BTC then ETH
    tmpl, err = template.New("keyprint").Parse(PKey_BTCAddr_Message2_Template)
    if err != nil { return "", err }
    buf := new(bytes.Buffer)
    err = tmpl.Execute(buf, pkey)
    pkey.Message2 = MX_Message(buf.String())
    tmpl, err = template.New("keyprint").Parse(PKey_BTCAddr_Message3_Template)
    if err != nil { return "", err }
    buf = new(bytes.Buffer)
    err = tmpl.Execute(buf, pkey)
    pkey.Message3 = MX_Message(buf.String())
    tmpl, err = template.New("keyprint").Parse(PKey_ETHAddr_Message4_Template)
    if err != nil { return "", err }
    buf = new(bytes.Buffer)
    err = tmpl.Execute(buf, pkey)
    pkey.Message4 = MX_Message(buf.String())
    tmpl, err = template.New("keyprint").Parse(PKey_ETHAddr_Message5_Template)
    if err != nil { return "", err }
    buf = new(bytes.Buffer)
    err = tmpl.Execute(buf, pkey)
    pkey.Message5 = MX_Message(buf.String())
  } else if printbtc {
    // Only print BTC
    tmpl, err := template.New("keyprint").Parse(PKey_BTCAddr_Message4_Template)
    if err != nil { return "", err }
    buf := new(bytes.Buffer)
    err = tmpl.Execute(buf, pkey)
    pkey.Message4 = MX_Message(buf.String())
    tmpl, err = template.New("keyprint").Parse(PKey_BTCAddr_Message5_Template)
    if err != nil { return "", err }
    buf = new(bytes.Buffer)
    err = tmpl.Execute(buf, pkey)
    pkey.Message5 = MX_Message(buf.String())
  } else if printeth {
    // Only print ETH
    tmpl, err := template.New("keyprint").Parse(PKey_ETHAddr_Message4_Template)
    if err != nil { return "", err }
    buf := new(bytes.Buffer)
    err = tmpl.Execute(buf, pkey)
    pkey.Message4 = MX_Message(buf.String())
    tmpl, err = template.New("keyprint").Parse(PKey_ETHAddr_Message5_Template)
    if err != nil { return "", err }
    buf = new(bytes.Buffer)
    err = tmpl.Execute(buf, pkey)
    pkey.Message5 = MX_Message(buf.String())
  }
  pkey.Layout = PKey_Layout(pkey)
  tmpl, err = template.New("keyprint").Parse(PKey_Print_Template)
  if err != nil { return "", err }
  buf = new(bytes.Buffer)
  err = tmpl.Execute(buf, pkey)
  return strings.ToUpper(buf.String()), nil
}

func KeyPrint_PKey_XMR(pkey PKey_XMR) (string, error) {
  tmpl, err := template.New("keyprint").Parse(PKey_MNTDate_Template)
  if err != nil { return "", err }
  buf := new(bytes.Buffer)
  err = tmpl.Execute(buf, pkey)
  pkey.MNTMessage = MX_Message(buf.String())
  tmpl, err = template.New("keyprint").Parse(PKey_XMR_Addr_Message2_Template)
  if err != nil { return "", err }
  buf = new(bytes.Buffer)
  err = tmpl.Execute(buf, pkey)
  pkey.Message2 = MX_Message(buf.String())
  tmpl, err = template.New("keyprint").Parse(PKey_XMR_Addr_Message3_Template)
  if err != nil { return "", err }
  buf = new(bytes.Buffer)
  err = tmpl.Execute(buf, pkey)
  pkey.Message3 = MX_Message(buf.String())
  tmpl, err = template.New("keyprint").Parse(PKey_XMR_Addr_Message4_Template)
  if err != nil { return "", err }
  buf = new(bytes.Buffer)
  err = tmpl.Execute(buf, pkey)
  pkey.Message4 = MX_Message(buf.String())
  tmpl, err = template.New("keyprint").Parse(PKey_XMR_Addr_Message5_Template)
  if err != nil { return "", err }
  buf = new(bytes.Buffer)
  err = tmpl.Execute(buf, pkey)
  pkey.Message5 = MX_Message(buf.String())
  pkey.Layout = PKey_XMR_Layout(pkey)
  tmpl, err = template.New("keyprint").Parse(PKey_Print_Template)
  if err != nil { return "", err }
  buf = new(bytes.Buffer)
  err = tmpl.Execute(buf, pkey)
  return strings.ToUpper(buf.String()), nil
}

const PKey_Print_Template = `
<]F3 SY540SX860
{{.Layout}}>

<{{.Message}}
{{.MNTMessage}}
{{.Message2}}
{{.Message3}}
{{.Message4}}
{{.Message5}}>
`

func KeyPrint_C330_Validate(str string) (error) {
  if len(str) > 26 {
    return errors.New("String is longer than maximum (26) : " + string(len(str)) + " : " + str)
  }
  for _, c := range strings.ToUpper(str) {
    if !(strings.ContainsRune(C330CharSet, c)) {
      return errors.New("String contains character outside of the C330 character set. : " + string(c) + " : " + str)
    }
  }
  return nil
}

func KeyPrint_PKey_BIP39(pkey PKey_BIP39) (string, string, error) {
  for _, mnemonic := range pkey.Mnemonic {
    if len(mnemonic) > 11 {
      return "", "", errors.New("Mnemonic was longer than max length. (11) : " + mnemonic)
    }
    if strings.ToUpper(mnemonic) != mnemonic {
      return "", "", errors.New("Mnemonic was not in full upper case. : " + mnemonic)
    }
    err := KeyPrint_C330_Validate(mnemonic)
    if err != nil { return "", "", err }
  }
  if bip39.IsMnemonicValid(strings.ToLower(strings.Join(pkey.Mnemonic[:], " "))) == false {
    return "", "", errors.New("Mnemonic was not valid. : " + strings.ToLower(strings.Join(pkey.Mnemonic[:], " ")))
  }
  tmpl, err := template.New("keyprint").Parse(PKey_BIP39_Print_Template0)
  if err != nil { return "", "", err }
  keyprint_buf := new(bytes.Buffer)
  err = tmpl.Execute(keyprint_buf, pkey)
  if err != nil { return "", "", err }
  str0 := strings.ToUpper(keyprint_buf.String())
  tmpl, err = template.New("keyprint").Parse(PKey_BIP39_Print_Template1)
  if err != nil { return "", "", err }
  keyprint_buf = new(bytes.Buffer)
  err = tmpl.Execute(keyprint_buf, pkey)
  if err != nil { return "", "", err }
  str1 := strings.ToUpper(keyprint_buf.String())
  return str1, str0, err
}

func KeyPrint_PKey_XMR_Mnemonic(pkey PKey_XMR_Mnemonic) (string, string, string, error) {
  for _, mnemonic := range pkey.Mnemonic {
    if len(mnemonic) > 12 {
      return "", "", "", errors.New("Mnemonic was longer than max length. (12) : " + mnemonic)
    }
    if strings.ToUpper(mnemonic) != mnemonic {
      return "", "", "", errors.New("Mnemonic was not in full upper case. : " + mnemonic)
    }
    err := KeyPrint_C330_Validate(mnemonic)
    if err != nil { return "", "", "", err }
  }
  tmpl, err := template.New("keyprint").Parse(PKey_XMR_Mnemonic_Print_Template0)
  if err != nil { return "", "", "", err }
  keyprint_buf := new(bytes.Buffer)
  err = tmpl.Execute(keyprint_buf, pkey)
  if err != nil { return "", "", "", err }
  str0 := strings.ToUpper(keyprint_buf.String())
  tmpl, err = template.New("keyprint").Parse(PKey_XMR_Mnemonic_Print_Template1)
  if err != nil { return "", "", "", err }
  keyprint_buf = new(bytes.Buffer)
  err = tmpl.Execute(keyprint_buf, pkey)
  if err != nil { return "", "", "", err }
  str1 := strings.ToUpper(keyprint_buf.String())
  tmpl, err = template.New("keyprint").Parse(PKey_XMR_Mnemonic_Print_Template2)
  if err != nil { return "", "", "", err }
  keyprint_buf = new(bytes.Buffer)
  err = tmpl.Execute(keyprint_buf, pkey)
  if err != nil { return "", "", "", err }
  str2 := strings.ToUpper(keyprint_buf.String())
  return str2, str1, str0, err
}

func KeyPrint_PKey_Info() (string, error) {
  tmpl, err := template.New("keyprint").Parse(PKey_Info_Print_Template)
  if err != nil { return "", err }
  keyprint_buf := new(bytes.Buffer)
  err = tmpl.Execute(keyprint_buf, nil)
  if err != nil { return "", err }
  return strings.ToUpper(keyprint_buf.String()), nil
}

func KeyPrint_PKey_XMR_Info() (string, error) {
  tmpl, err := template.New("keyprint").Parse(PKey_XMR_Info_Print_Template)
  if err != nil { return "", err }
  keyprint_buf := new(bytes.Buffer)
  err = tmpl.Execute(keyprint_buf, nil)
  if err != nil { return "", err }
  return strings.ToUpper(keyprint_buf.String()), nil
}

func KeyPrint(
  mnemonic [24]string,
  xmrmnemonic [25]string,
  btcaddr string,
  ethaddr string,
  xmraddr string,
  btcheader string,
  ethheader string,
  xmrheader string,
  message string,
  message2 string,
  message3 string,
  message4 string,
  message5 string,
  xmrmessage string,
  printbtc bool,
  printeth bool,
  skipinfo bool,
  skipmnemonic bool,
  skipmnt bool,
  skipxmr bool) (string, string, string, string, string, string, string, string, string, error) {
  var err error
  str0 := ""
  if skipinfo == false {
    str0, err = KeyPrint_PKey_Info()
    if err != nil { return "", "", "", "", "", "", "", "", "", err }
  }
  str1 := ""
  str2 := ""
  if skipmnemonic == false {
    for i, word := range mnemonic {
      mnemonic[i] = strings.ToUpper(word)
    }
    pkey_bip39 := PKey_BIP39{Mnemonic: mnemonic}
    str1, str2, err = KeyPrint_PKey_BIP39(pkey_bip39)
    if err != nil { return "", "", "", "", "", "", "", "", "", err }
  }
  mntdate := time.Now().UTC().Format("2006-01-02")
  str3 := ""
  if skipmnt == false {
    pkey := PKey{
      BTCAddr: btcaddr,
      BTCHeader: strings.ToUpper(btcheader),
      ETHAddr: ethaddr,
      ETHHeader: strings.ToUpper(ethheader),
      Message: strings.ToUpper(message),
      Message2: strings.ToUpper(message2),
      Message3: strings.ToUpper(message3),
      Message4: strings.ToUpper(message4),
      Message5: strings.ToUpper(message5),
      MNTDate: mntdate}
    str3, err = KeyPrint_PKey(pkey, printbtc, printeth)
    if err != nil { return "", "", "", "", "", "", "", "", "", err }
  }
  str4 := ""
  str5 := ""
  str6 := ""
  str7 := ""
  str8 := ""
  if skipxmr == false {
    str4, err = KeyPrint_PKey_XMR_Info()
    if err != nil { return "", "", "", "", "", "", "", "", "", err }
    pkey_xmr_mnemonic := PKey_XMR_Mnemonic{Mnemonic: xmrmnemonic}
    str5, str6, str7, err = KeyPrint_PKey_XMR_Mnemonic(pkey_xmr_mnemonic)
    if err != nil { return "", "", "", "", "", "", "", "", "", err }
    pkey_xmr := PKey_XMR{
      Addr: xmraddr,
      Header: strings.ToUpper(xmrheader),
      Message: strings.ToUpper(xmrmessage),
      MNTDate: mntdate,
    }
    str8, err = KeyPrint_PKey_XMR(pkey_xmr)
    if err != nil { return "", "", "", "", "", "", "", "", "", err }
  }
  return str0, str1, str2, str3, str4, str5, str6, str7, str8, nil
}

func hashSha256(data []byte) ([]byte, error) {
	hasher := sha256.New()
	_, err := hasher.Write(data)
	if err != nil {
		return nil, err
	}
	return hasher.Sum(nil), nil
}

func hashKeccak256(data []byte) ([]byte, error) {
	hasher := sha3.NewLegacyKeccak256()
	_, err := hasher.Write(data)
	if err != nil {
		return nil, err
	}
	return hasher.Sum(nil), nil
}

func hashDoubleSha256(data []byte) ([]byte, error) {
	hash1, err := hashSha256(data)
	if err != nil {
		return nil, err
	}

	hash2, err := hashSha256(hash1)
	if err != nil {
		return nil, err
	}
	return hash2, nil
}

func hashRipeMD160(data []byte) ([]byte, error) {
	hasher := ripemd160.New()
	_, err := io.WriteString(hasher, string(data))
	if err != nil {
		return nil, err
	}
	return hasher.Sum(nil), nil
}

func hash160(data []byte) ([]byte, error) {
	hash1, err := hashSha256(data)
	if err != nil {
		return nil, err
	}

	hash2, err := hashRipeMD160(hash1)
	if err != nil {
		return nil, err
	}

	return hash2, nil
}


func checksum(data []byte) ([]byte, error) {
	hash, err := hashDoubleSha256(data)
	if err != nil {
		return nil, err
	}

	return hash[:4], nil
}

func addChecksumToBytes(data []byte) ([]byte, error) {
	checksum, err := checksum(data)
	if err != nil {
		return nil, err
	}
	return append(data, checksum...), nil
}

func base58Encode(data []byte) string {
	return BitcoinBase58Encoding.EncodeToString(data)
}

func base58CheckEncode(data []byte) string {
	return "1"+BitcoinBase58Encoding.EncodeToString(data)
}

func base58Decode(data string) ([]byte, error) {
	return BitcoinBase58Encoding.DecodeString(data)
}

// As described at https://crypto.stackexchange.com/a/8916
func expandPublicKey(key []byte) (*big.Int, *big.Int) {
	Y := big.NewInt(0)
	X := big.NewInt(0)
	X.SetBytes(key[1:])

	// y^2 = x^3 + ax^2 + b
	// a = 0
	// => y^2 = x^3 + b
	ySquared := big.NewInt(0)
	ySquared.Exp(X, big.NewInt(3), nil)
	ySquared.Add(ySquared, curveParams.B)

	Y.ModSqrt(ySquared, curveParams.P)

	Ymod2 := big.NewInt(0)
	Ymod2.Mod(Y, big.NewInt(2))

	signY := uint64(key[0]) - 2
	if signY != Ymod2.Uint64() {
		Y.Sub(curveParams.P, Y)
	}

	return X, Y
}

func xmr_mnemonic(spend_key []byte) [25]string {

	w := [25]string{}

	// Mnemonics convert on a ratio of 4:3 minimum: four bytes creates three words, plus one checksum word
	size := uint32(len(XMRWordsEnglish))
	for i := 0; i < 32; i += 4 {
		x := binary.LittleEndian.Uint32(spend_key[i : i+4])
		w1 := x % size
		w2 := (x/size + w1) % size
		w3 := (x/size/size + w2) % size
		w[i/4*3] = XMRWordsEnglish[w1]
		w[i/4*3+1] = XMRWordsEnglish[w2]
		w[i/4*3+2] = XMRWordsEnglish[w3]
	}

	// checksum word
	h := crc32.NewIEEE()
	for _, v := range w[:24] {
		r := string([]rune(v)[:3])
		h.Write([]byte(r))
	}
	sum := h.Sum32()
	idx := sum % 24
	w[24] = w[idx]

	return w
}

func priv_key_to_pub(priv []byte) []byte {
	point := edwards25519.NewIdentityPoint()
	scalar := edwards25519.NewScalar()
	scalar.SetCanonicalBytes(priv)
	public_key_point := point.ScalarBaseMult(scalar)
	public_key_bytes := public_key_point.Bytes()
	return public_key_bytes
}

func cn_fast_hash(data []byte) [32]byte {
  bytes, _ := hashKeccak256(data)
  return [32]byte(bytes)
}

func sc_reduce32(data [32]byte) []byte {
	scalar := edwards25519.NewScalar()
	zeros := [64]byte{0}
	for i := 0; i < 32; i += 1 {
		zeros[i] = data[i]
	}
	scalar.SetUniformBytes(zeros[:])
	return scalar.Bytes()
}

func get_public_address(public_spend_key []byte, public_view_key []byte) string {
	comb := []byte{18}
	comb = append(comb, public_spend_key...)
	comb = append(comb, public_view_key...)
	//fmt.Println(comb)
	hash := cn_fast_hash(comb)
	//fmt.Println(comb)
	//fmt.Println(hash[0:4])
	t := append(comb, hash[0:4]...)
	//fmt.Println(t)
	return monero_base58(t)
}

func monero_base58(data []byte) string {
	t := ""
	for i := 0; i < len(data)/8; i++ {
		b := BitcoinBase58Encoding.EncodeToString(data[i*8 : i*8+8])
		if (len(b) % 11) != 0 {
			t += strings.Repeat("1", 11-(len(b)%11))
		}
		t += b
	}
	t += BitcoinBase58Encoding.EncodeToString(data[len(data)-(len(data)%8) : len(data)])
	return t
}

//
// Numerical
//
func uint32Bytes(i uint32) []byte {
	bytes := make([]byte, 4)
	binary.BigEndian.PutUint32(bytes, i)
	return bytes
}

func genMnemonic() (string, error) {
  entropy, err := bip39.NewEntropy(256)
  if err != nil { return "", err }
  mnemonic, err := bip39.NewMnemonic(entropy)
  if err != nil { return "", err }
  return mnemonic, nil
}

func genBTCAddr(mnemonic string) (string, error) {
  seed := bip39.NewSeed(mnemonic, "")
  masterKey, err := bip32.NewMasterKey(seed)
  if err != nil { return "", err }
  child, err := masterKey.NewChildKey(0x8000002C) // BIP 44 Purpose
  if err != nil { return "", err }
  child, err = child.NewChildKey(0x80000000) // BitCoin
  if err != nil { return "", err }
  child, err = child.NewChildKey(0x80000000) // Account
  if err != nil { return "", err }
  child, err = child.NewChildKey(0x0) // Chain (Visible)
  if err != nil { return "", err }
  //fmt.Println("BTC Extended key: ", child)
  child, err = child.NewChildKey(0x0) // First One.
  if err != nil { return "", err }
  //fmt.Println("BTC private key: ", hex.EncodeToString(child.Key))
  //fmt.Println("BTC public key: ", hex.EncodeToString(child.PublicKey().Key))
  hash, err := hash160(child.PublicKey().Key)
  if err != nil { return "", err }
  address, _ := addChecksumToBytes(append([]byte{0}, hash...))
  //fmt.Println("BTC public address: ", base58CheckEncode(address))
  return base58CheckEncode(address), nil
}

func genBTC84Addr(mnemonic string) (string, error) {
  seed := bip39.NewSeed(mnemonic, "")
  masterKey, err := bip32.NewMasterKey(seed)
  if err != nil { return "", err }
  child, err := masterKey.NewChildKey(0x80000054) // BIP 84 Purpose
  if err != nil { return "", err }
  child, err = child.NewChildKey(0x80000000) // BitCoin
  if err != nil { return "", err }
  child, err = child.NewChildKey(0x80000000) // Account
  if err != nil { return "", err }
  child, err = child.NewChildKey(0x0) // Chain (Visible)
  if err != nil { return "", err }
  //fmt.Println("BTC Extended key: ", child)
  child, err = child.NewChildKey(0x0) // First One.
  if err != nil { return "", err }
  //fmt.Println("BTC private key: ", hex.EncodeToString(child.Key))
  //fmt.Println("BTC public key: ", hex.EncodeToString(child.PublicKey().Key))
  hash, err := hash160(child.PublicKey().Key)
  if err != nil { return "", err }
  bits5, err := bech32.ConvertBits(hash, 8, 5, true)
  if err != nil { return "", err }
  bitsw := append([]byte{0}, bits5...)
  p2wkh, err := bech32.Bech32Encode("bc", bitsw)
  if err != nil { return "", err }
  return p2wkh, nil
}

func genETHAddr(mnemonic string) (string, error) {
  seed := bip39.NewSeed(mnemonic, "")
  masterKey, err := bip32.NewMasterKey(seed)
  if err != nil { return "", err }
  child, err := masterKey.NewChildKey(0x8000002C) // BIP 44 Purpose
  if err != nil { return "", err }
  child, err = child.NewChildKey(0x8000003C) // Ethereum
  if err != nil { return "", err }
  child, err = child.NewChildKey(0x80000000) // Account
  if err != nil { return "", err }
  child, err = child.NewChildKey(0x0) // Chain (Visible)
  if err != nil { return "", err }
  //fmt.Println("ETH Extended key: ", child)
  child, err = child.NewChildKey(0x0) // First One.
  if err != nil { return "", err }
  //fmt.Println("ETH private key: ", "0x" + hex.EncodeToString(child.Key))
  //fmt.Println("ETH public key: ", "0x" + hex.EncodeToString(child.PublicKey().Key))
  x, y := expandPublicKey(child.PublicKey().Key)
  hash, err := hashKeccak256(append(x.Bytes(), y.Bytes()...))
  if err != nil { return "", err }
  address2 := hex.EncodeToString(hash[12:])
  keccack256, err := hashKeccak256([]byte(address2))
  hexk256 := hex.EncodeToString(keccack256)
  address3 := ""
  if err != nil { return "", err }
  for i, c := range(address2) {
    if !unicode.IsDigit(c) && hexk256[i] >= '8' {
      address3 += string(unicode.ToUpper(c))
    } else {
      address3 += string(c)
    }
  }
  return "0x" + address3, nil
}

// Monero generates the spend keys and then the mnemonic.
func genXMRAddr() (string, string, error) {
  seed := [32]byte{0}
  _, err := rand.Read(seed[:])
  if err != nil { return "", "", err }
  spend_key := sc_reduce32(seed)
  keccak256, err := hashKeccak256(spend_key)
  if err != nil { return "", "", err }
  view_key := sc_reduce32([32]byte(keccak256))
  mnemonic := xmr_mnemonic(spend_key)
  public_spend_key := priv_key_to_pub(spend_key)
  public_view_key := priv_key_to_pub(view_key)
  address := get_public_address(public_spend_key, public_view_key)
  return strings.ToUpper(strings.Join(mnemonic[:], " ")), address, nil
}

func main() {
  mnemonic_p := flag.String("mnemonic", "", "twenty four colon separated mnemonics")
  xmrmnemonic_p := flag.String("xmrmnemonic", "", "twenty five colon separated mnemonics")
  btcaddr_p := flag.String("btcaddr", "", "bitcoin address")
  ethaddr_p := flag.String("ethaddr", "", "ethereum address")
  xmraddr_p := flag.String("xmraddr", "", "monero address")
  message_p := flag.String("message", "", "custom message")
  message2_p := flag.String("message2", "", "custom message (only used when printbtc or printeth are false)")
  message3_p := flag.String("message3", "", "custom message (only used when printbtc or printeth are false)")
  message4_p := flag.String("message4", "", "custom message (only used when printbtc and printeth are false)")
  message5_p := flag.String("message5", "", "custom message (only used when printbtc and printeth are false)")
  xmrmessage_p := flag.String("xmrmessage", "", "custom message (only used when skipxmr is false)")
  printbtc_p := flag.Bool("printbtc", true, "print the bitcoin address")
  printeth_p := flag.Bool("printeth", true, "print the ethereum address")
  btcheader_p := flag.String("btcheader", "BTC", "the bitcoin header")
  ethheader_p := flag.String("ethheader", "eth", "the ethereum header")
  xmrheader_p := flag.String("xmrheader", "xmr", "the monero header")
  keygen_p := flag.Bool("keygen", false, "randomly generate mnemonics")
  output_p := flag.String("output", "/dev/stdout", "output file")
  btcinitial_p := flag.String("btcinitial", "", "initial for bitcoin address")
  ethinitial_p := flag.String("ethinitial", "", "initial for ethereum address")
  xmrinitial_p := flag.String("xmrinitial", "", "initial for monero address")
  btclength_p := flag.Int("btclength", 0, "bitcoin address length (usually 34, can be less, 0 is any length)")
  skipinfo_p := flag.Bool("skipinfo", false, "skip output of public info")
  skipmnemonic_p := flag.Bool("skipmnemonic", false, "skip output of private mnemonics")
  skipmnt_p := flag.Bool("skipmnt", false, "skip output of public addresses")
  skipxmr_p := flag.Bool("skipxmr", true, "skip output of monero addresses")
  printqr_p := flag.Bool("printqr", false, "output a qr of the public addresses")
  flag.Parse()

  mnemonic := *mnemonic_p
  xmrmnemonic := *xmrmnemonic_p
  btcaddr := *btcaddr_p
  ethaddr := *ethaddr_p
  xmraddr := *xmraddr_p
  message := *message_p
  message2 := *message2_p
  message3 := *message3_p
  message4 := *message4_p
  message5 := *message5_p
  xmrmessage := *xmrmessage_p
  printbtc := *printbtc_p
  printeth := *printeth_p
  btcheader := *btcheader_p
  ethheader := *ethheader_p
  xmrheader := *xmrheader_p
  keygen := *keygen_p
  output := *output_p
  ethinitial := "0x" + *ethinitial_p
  btcinitial := "bc1" + *btcinitial_p
  xmrinitial := *xmrinitial_p
  btclength := *btclength_p
  skipinfo := *skipinfo_p
  skipmnemonic := *skipmnemonic_p
  skipmnt := *skipmnt_p
  skipxmr := *skipxmr_p
  printqr := *printqr_p

  if (mnemonic == "") && (keygen == false) {
    flag.PrintDefaults()
    return
  }

  mnemonic = strings.Join(strings.Split(mnemonic, ":")[0:], " ")
  xmrmnemonic = strings.Join(strings.Split(xmrmnemonic, ":")[0:], " ")

  var err error
  if keygen == true {
    mnemonic, err = genMnemonic()
    if err != nil { panic(err) }
    ethaddr, err = genETHAddr(mnemonic)
    if err != nil { panic(err) }
    btcaddr, err = genBTC84Addr(mnemonic)
    if err != nil { panic(err) }
    xmrmnemonic, xmraddr, err = genXMRAddr()
    if err != nil { panic(err) }
    for i := 0; i < 100000000; i++ {
      if !skipmnt && skipmnemonic {
        mnemonic, err = genMnemonic()
        if err != nil { panic(err) }
      }
      if !skipmnt && printeth {
        ethaddr, err = genETHAddr(mnemonic)
        if err != nil { panic(err) }
        if !strings.HasPrefix(ethaddr, ethinitial) { continue }
      }
      if !skipmnt && printbtc {
        btcaddr, err = genBTC84Addr(mnemonic)
        if err != nil { panic(err) }
        if !strings.HasPrefix(btcaddr, btcinitial) { continue }
        if (btclength != 0) && (len(btcaddr) != btclength) { continue }
      }
      if !skipxmr {
        xmrmnemonic, xmraddr, err = genXMRAddr()
        if err != nil { panic(err) }
        if !strings.HasPrefix(xmraddr, xmrinitial) { continue }
      }
      break
    }
  } else {
    ethaddr, err = genETHAddr(mnemonic)
    if err != nil { panic(err) }
    btcaddr, err = genBTC84Addr(mnemonic)
    if err != nil { panic(err) }
  }
  str0, str1, str2, str3, str4, str5, str6, str7, str8, err := KeyPrint(
    *(*[24]string)(strings.Split(mnemonic, " ")[0:24][:]),
    *(*[25]string)(strings.Split(xmrmnemonic, " ")[0:25][:]),
    btcaddr,
    ethaddr,
    xmraddr,
    btcheader,
    ethheader,
    xmrheader,
    message,
    message2,
    message3,
    message4,
    message5,
    xmrmessage,
    printbtc,
    printeth,
    skipinfo,
    skipmnemonic,
    skipmnt,
    skipxmr)
  if err != nil { panic(err) }
  input := bufio.NewScanner(os.Stdin)
  f, err := os.OpenFile(output, os.O_SYNC|os.O_CREATE|os.O_RDWR, 0644)
  if err != nil { panic(err) }
  if str0 != "" {
    _, err = f.WriteString(str0)
    if err != nil { panic(err) }
    f.Sync()
    input.Scan()
  }
  if str1 != "" {
    _, err = f.WriteString(str1)
    if err != nil { panic(err) }
    f.Sync()
    input.Scan()
  }
  if str2 != "" {
    _, err = f.WriteString(str2)
    if err != nil { panic(err) }
    f.Sync()
    input.Scan()
  }
  if str3 != "" {
    _, err = f.WriteString(str3)
    if err != nil { panic(err) }
    f.Sync()
    input.Scan()
  }
  if str4 != "" {
    _, err = f.WriteString(str4)
    if err != nil { panic(err) }
    f.Sync()
    input.Scan()
  }
  if str5 != "" {
    _, err = f.WriteString(str5)
    if err != nil { panic(err) }
    f.Sync()
    input.Scan()
  }
  if str6 != "" {
    _, err = f.WriteString(str6)
    if err != nil { panic(err) }
    f.Sync()
    input.Scan()
  }
  if str7 != "" {
    _, err = f.WriteString(str7)
    if err != nil { panic(err) }
    f.Sync()
    input.Scan()
  }
  if str8 != "" {
    _, err = f.WriteString(str8)
    if err != nil { panic(err) }
    f.Sync()
    input.Scan()
  }
  if printqr {
    println("BTC QR")
    qrterminal.Generate(btcaddr, qrterminal.M, os.Stderr)
    println("ETH QR")
    qrterminal.Generate(ethaddr, qrterminal.M, os.Stderr)
    println("XMR QR")
    qrterminal.Generate(xmraddr, qrterminal.M, os.Stderr)
    println("Done.")
  }
}
