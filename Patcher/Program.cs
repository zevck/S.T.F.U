using Mutagen.Bethesda;
using Mutagen.Bethesda.Synthesis;
using Mutagen.Bethesda.Skyrim;
using Mutagen.Bethesda.Plugins;
using Mutagen.Bethesda.Plugins.Cache;
using System;
using System.Linq;
using System.Collections.Generic;
using System.IO;
using YamlDotNet.Serialization;
using YamlDotNet.Serialization.NamingConventions;

namespace STFU
{
    public class Program
    {
        public static async Task<int> Main(string[] args)
        {
            // Let Synthesis auto-detect game release from command line
            return await SynthesisPipeline.Instance
                .AddPatch<ISkyrimMod, ISkyrimModGetter>(RunPatch)
                .Run(args);
        }

        public static void RunPatch(IPatcherState<ISkyrimMod, ISkyrimModGetter> state)
        {
            Console.WriteLine("STFU: Dialogue blocking patcher starting...");
            
            // Get executable directory for config files
            var executableDir = AppContext.BaseDirectory;
            
            // Verify STFU.esp is in load order
            var stfuInLoadOrder = state.LoadOrder.ListedOrder.Any(m => 
                m.ModKey.FileName.String.Equals("STFU.esp", StringComparison.OrdinalIgnoreCase));
            
            if (!stfuInLoadOrder)
            {
                Console.WriteLine("ERROR: STFU.esp not found in load order!");
                Console.WriteLine("STFU.esp must be:");
                Console.WriteLine("  1. Installed as a mod (contains ~60 globals for MCM toggles)");
                Console.WriteLine("  2. Enabled in your mod manager");
                Console.WriteLine("  3. Placed BEFORE Synthesis.esp in load order");
                Console.WriteLine("");
                Console.WriteLine("Without STFU.esp, the patcher cannot write MCM-toggleable conditions.");
                Console.WriteLine("The patcher will continue but all dialogue will be permanently blocked (no runtime toggles).");
                Console.WriteLine("");
            }
            
            // Quest whitelists (populated from YAML only)
            var whitelistedQuestEditorIds = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            var whitelistedQuestFormKeys = new HashSet<FormKey>();
            
            // Hardcoded ambient scene EditorIDs (instead of pattern matching)
            // These are vanilla + DLC ambient scenes that should be blockable
            var hardcodedSceneEditorIds = new HashSet<string>(StringComparer.OrdinalIgnoreCase)
            {
                "AddvarHouse2", "AddvarHouseScene1", "AncanoMirabelleScene01", "AngasMillCommonHouseScene01", "AngasMillCommonHouseScene02",
                "ApothecaryScene", "ArgonianScene1", "ArnielBrelynaScene01", "ArnielEnthirScene01", "ArnielEnthirScene02",
                "ArnielEnthirSceneDragon01", "ArnielNiryaScene01", "ArnielNiryaScene02", "AtWork", "BardsLunch",
                "BetridBoliMarketScene1", "BetridBoliMarketScene2", "BetridKerahMarketScene2", "BetridKerahScene1", "BirnaEnthirScene01",
                "BirnaEnthirScene02", "BlacksmithConversation", "BrelynaOnmundDormScene01", "BrelynaOnmundDormScene2", "BrinasHouseScene01",
                "BrinasHouseScene02", "BrylingFalkTryst", "BrylingScene1", "BrylingScene2", "BuyingARound",
                "CaravanScene1Scene", "CaravanScene2Scene", "CaravanScene3Scene", "CaravanScene4Scene", "CaravanScene5Scene",
                "CaravanScene6Scene", "CaravanScene7Scene", "CaravanScene8Scene", "CidhnaMinePrisoner01Scene", "CidhnaMinePrisoner03",
                "CidhnaMinePrisonerScene02", "CidhnaMinePrisonerScene04", "ColetteDrevisScene01", "CollegeGHallScene06", "CommanderScene1",
                "CommanderScene2", "CourtScene2", "DamphallMine01SceneQuestDialogue", "DawnstarIntroBrinaScene",
                "DGScene04", "DGSceneSpecial01", "DialogueBrandyMugFarmScene1", "DialogueBrandyMugFarmScene2", "DialogueBrandyMugFarmScene3",
                "DialogueCidhnaMineBlathlocGrisvar02Scene", "DialogueCompanionsAelaNjadaScene1", "DialogueCompanionsAelaNjadaScene2", "DialogueCompanionsFarkasAthisScene1", "DialogueCompanionsFarkasAthisScene2",
                "DialogueCompanionsFarkasTorvarScene1", "DialogueCompanionsFarkasTorvarScene2", "DialogueCompanionsKodlakAelaScene1", "DialogueCompanionsKodlakAelaScene2", "DialogueCompanionsKodlakFarkasScene1",
                "DialogueCompanionsKodlakFarkasScene2", "DialogueCompanionsKodlakSkjorScene1", "DialogueCompanionsKodlakSkjorScene2", "DialogueCompanionsKodlakTorvarScene1", "DialogueCompanionsKodlakTorvarScene2",
                "DialogueCompanionsRiaVilkasScene1", "DialogueCompanionsRiaVilkasScene2", "DialogueCompanionsRiaVilkasScene3", "DialogueCompanionsSkjorAelaScene1", "DialogueCompanionsSkjorAelaScene2",
                "DialogueCompanionsSkjorNjadaScene1", "DialogueCompanionsSkjorNjadaScene2", "DialogueCompanionsTorvarAthisScene1", "DialogueCompanionsTorvarAthisScene2", "DialogueDarkwaterCrossingAnnekeVernerScene1",
                "DialogueDarkwaterCrossingAnnekeVernerScene2", "DialogueDarkwaterCrossingAnnekeVernerScene3", "DialogueDarkwaterCrossingHrefna1", "DialogueDarkwaterCrossingHrefna2", "DialogueDarkwaterCrossingHrefna4",
                "DialogueDarkwaterCrossingInn4", "DialogueDarkwaterCrossingInn5", "DialogueDawnstarWindpeakInnScene04View", "DialogueDawnstarWindpeakInnScene05View", "DialogueDawnstarWindpeakInnScene06View",
                "DialogueDawnstarWindpeakInnScene07View", "DialogueDawnstarWindpeakInnScene08View", "DialogueDawnstarWindpeakInnScene09View", "DialogueDushnikhYalBlacksmithingScene01View", "DialogueDushnikhYalBlacksmithingScene02View",
                "DialogueDushnikhYalBlacksmithingScene03View", "DialogueDushnikhYalExterior01Scene", "DialogueDushnikhYalExterior02Scene", "DialogueDushnikhYalLonghouse01Scene", "DialogueDushnikhYalLonghouse02Scene",
                "DialogueDushnikhYalLonghouse03Scene", "DialogueDushnikhYalMine01Scene", "DialogueDushnikhYalMine02Scene", "DialogueGenericSceneDog01Scene", "DialogueHlaaluFarmScene1",
                "DialogueHlaaluFarmScene2", "DialogueHlaaluFarmScene3", "DialogueHollyfrostFarmTulvurHilleviScene1", "DialogueHollyfrostFarmTulvurHilleviScene2", "DialogueHollyfrostFarmTulvurTorstenScene1",
                "DialogueHollyfrostFarmTulvurTorstenScene2", "DialogueIvarsteadFellstarFarmScene01SCN", "DialogueIvarsteadFellstarFarmScene02SCN", "DialogueIvarsteadFellstarFarmScene03SCN", "DialogueIvarsteadInnScene01SCN",
                "DialogueIvarsteadInnScene02SCN", "DialogueIvarsteadInnScene03SCN", "DialogueIvarsteadInnScene04SCN", "DialogueIvarsteadInnScene05SCN", "DialogueIvarsteadInnScene06SCN",
                "DialogueIvarsteadKlimmekHouseScene01SCN", "DialogueIvarsteadKlimmekHouseScene02SCN", "DialogueIvarsteadTembasMillScene01SCN", "DialogueIvarsteadTembasMillScene02SCN", "DialogueKarthwastenMinersBarracksScene01View",
                "DialogueKarthwastenMinersBarracksScene02view", "DialogueKarthwastenMinersBarracksScene03View", "DialogueKarthwastenMinersBarracksScene04View", "DialogueKarthwastenMinersBarracksScene05View", "DialogueKarthwastenMinersBarracksScene06View",
                "DialogueKarthwastenMinersBarracksScene07View", "DialogueKarthwastenMinersBarracksScene08View", "DialogueKarthwastenMinersHouseScene03View", "DialogueKarthwastenOutsideScene01View", "DialogueKarthwastenOutsideScene02View",
                "DialogueKarthwastenSanuarachMineScene04View", "DialogueKarthwastenT01EnmonsHouseScene02View", "DialogueKarthwastenT01EnmonsHouseScene03View", "DialogueKarthwastenT01EnmonsHouseScene04View", "DialogueKolskeggrMineHouseScene02View",
                "DialogueKolskeggrMineScene01View", "DialogueKolskeggrMineScene02View", "DialogueKolskeggrMineScene03View", "DialogueKynesgroveEskilGala01TGCoKGScene", "DialogueKynesgroveEskilGala02TGCoKGScene",
                "DialogueKynesgroveEskilGala03TGCoKGScene", "DialogueKynesgroveEskilGala04TGCoKGScene", "DialoguekynesgroveEskilPilgrim01Scene01TGCoKGScene01", "DialogueKynesgroveGannaGemmaScene1", "DialogueKynesgroveGannaGemmaScene2",
                "DialogueKynesgroveGannaGemmaScene3", "DialogueKynesgroveGannaGemmaScene4", "DialogueKynesgroveGannaGemmaScene5", "DialogueKynesgroveGannaGemmaScene6", "DialogueKynesgroveKjeldDravyneaMineScene1",
                "DialogueKynesgroveKjeldDravyneaMineScene2", "DialogueKynesgroveKjeldDravyneaMineScene3", "DialogueKynesgroveKjeldYoungerIddraScene1", "DialogueKynesgroveKjeldYoungerIddraScene2", "DialogueKynesgroveKjeldYoungerIddraScene3",
                "DialogueKynesgroveKjeldYoungerKjeldScene1", "DialogueKynesgroveKjeldYoungerKjeldScene2", "DialogueKynesgroveKjeldYoungerKjeldScene3", "DialogueKynesgrovePilgrim01Pilgrim0201TGCoKGScene01", "DialogueKynesgrovePilgrim01Pilgrim0202TGCoKScene02",
                "DialogueKynesgrovePilgrim02Eskil01TGCoKGScene01", "DialogueKynesgroveRoggiDravyneaMineScene1", "DialogueKynesgroveRoggiDravyneaMineScene2", "DialogueKynesgroveRoggiIddraScene1", "DialogueKynesgroveRoggiIddraScene2",
                "DialogueKynesgroveRoggiIddraScene3", "DialogueKynesgroveRoggiKjeldInnScene1", "DialogueKynesgroveRoggiKjeldInnScene2", "DialogueKynesgroveRoggiKjeldInnScene3", "DialogueKynesgroveRoggiKjeldScene1",
                "DialogueKynesgroveRoggiKjeldScene2", "DialogueKynesgroveRoggiKjeldScene3", "DialogueLeftHandMineDaighresHouse01Scene", "DialogueLeftHandMineDaighresHouse02View", "DialogueLeftHandMineDaighreSkaggi02Scene",
                "DialogueLeftHandMineIntroSceneView", "DialogueLeftHandMineMinersBarracks01Scene", "DialogueLeftHandMineMinersBarracks02View", "DialogueLeftHandMineScene04View", "DialogueMarkarthArnleifandSonsScene05View",
                "DialogueMarkarthArnleifandSonsScene06View", "DialogueMarkarthBlacksmithScene01View", "DialogueMarkarthDragonsKeepScene01View", "DialogueMarkarthDragonsKeepScene02View", "DialogueMarkarthDragonsMarketScene01View",
                "DialogueMarkarthDragonsRiversideScene01View", "DialogueMarkarthGenericScene01View", "DialogueMarkarthGenericScene02View", "DialogueMarkarthGenericScene03View", "DialogueMarkarthHagsCureSceneMuiriBothela03Scene",
                "DialogueMarkarthHagsCureSceneMuiriBothela04Scene", "DialogueMarkarthIntroArnleifandSonsSceneView", "DialogueMarkarthIntroBlacksmithSceneView", "DialogueMarkarthIntroSmelterSceneView", "DialogueMarkarthKeepIntroCourtSceneView",
                "DialogueMarkarthPostAttackScene01View", "DialogueMarkarthRiversideScene01View", "DialogueMarkarthRiversideScene02View", "DialogueMarkarthRiversideScene03View", "DialogueMarkarthRiversideScene04View",
                "DialogueMarkarthRiversideScene07View", "DialogueMarkarthRiversideScene08View", "DialogueMarkarthRiversideScene09View", "DialogueMarkarthRiversideScene10View", "DialogueMarkarthRiversideScene11View",
                "DialogueMarkarthRiversideScene12View", "DialogueMarkarthSilverFishInnScene16View", "DialogueMarkarthSilverFishInnScene17View", "DialogueMarkarthStablesBanningCedranScene01View", "DialogueMarkarthStablesBanningCedranScene02View",
                "DialogueMorKhazgurExterior02Scene", "DialogueMorKhazgurHuntingScene02View", "DialogueMorKhazgurLonghouse01Scene", "DialogueMorKhazgurLonghouse02Scene", "DialogueNarzulburBolarYatulScene1",
                "DialogueNarzulburBolarYatulScene2", "DialogueNarzulburBolarYatulScene3", "DialogueNarzulburBolarYatulScene4", "DialogueNarzulburMauhulakhBolarScene1", "DialogueNarzulburMauhulakhBolarScene2",
                "DialogueNarzulburMauhulakhDushnamubScene1", "DialogueNarzulburMauhulakhDushnamubScene2", "DialogueNarzulburMauhulakhDushnamubScene3", "DialogueNarzulburMauhulakhUrogScene1", "DialogueNarzulburMauhulakhUrogScene2",
                "DialogueNarzulburMauhulakhUrogScene3", "DialogueNarzulburMauhulakhYatulScene1", "DialogueNarzulburMauhulakhYatulScene2", "DialogueNarzulburMulGadbaScene1", "DialogueNarzulburMulGadbaScene2",
                "DialogueNarzulburMulGadbaScene3", "DialogueNarzulburMulGadbaScene4", "DialogueNarzulburUrogYatulScene1", "DialogueNarzulburUrogYatulScene2", "DialogueNarzulburUrogYatulScene3",
                "DialogueOldHroldanHangedManInnScene05View", "DialogueOldHroldanHangedManInnScene06View", "DialogueRiftenGrandPlazaScene019",
                "DialogueRiftenSS02Scene", "DialogueRiverwoodSceneParts", "DialogueSalviusFarmSceneAView", "DialogueSalviusFarmSceneDView",
                "DialogueSalviusSceneCView", "DialogueSceneSigridAlvor", "DialogueSceneSigridEmbry", "DialogueSceneSigridHilde", "DialogueShorsStoneGatheringScene01SCN",
                "DialogueShorsStoneGatheringScene02SCN", "DialogueShorsStoneGatheringScene03SCN", "DialogueShorsStoneGatheringScene04SCN", "DialogueShorsStoneGatheringScene05SCN", "DialogueShorsStoneGatheringScene06SCN",
                "DialogueSolitudePalaceScene10ElisifBolgeir", "DialogueSolitudePalaceScene5FalkErikur", "DialogueSolitudePalaceScene6BrylingFalk", "DialogueSolitudePalaceScene7FalkBrylingSybille", "DialogueSolitudePalaceScene8ErikurFalkElisif",
                "DialogueSolitudePalaceScene9ElisifFalkBolgeir", "DialogueSoljundsMineHouseScene01View", "DialogueSoljundsMineHouseScene02View", "DialogueSoljundsMineHouseScene03View", "DialogueSoljundsMineHouseScene04View",
                "DialogueSoljundsMineScene01view", "DialogueSoljundsMineScene02view", "DialogueWhiterunAnoriathBrenuinScene1Scene", "DialogueWhiterunAnoriathOlfinaScene1Scene", "DialogueWhiterunAnoriathYsoldaScene1Scene",
                "DialogueWhiterunCarlottaBrenuinScene1Scene", "DialogueWhiterunCarlottaNazeemScene1View", "DialogueWhiterunCarlottaOlfinaScene1Scene", "DialogueWhiterunCarlottaYsoldaScene1Scene", "DialogueWhiterunDagnyFrothar1Scene",
                "DialogueWhiterunFraliaYsoldaScene1View", "DialogueWhiterunHrongarBalgruuf1Scene", "DialogueWhiterunHrongarProventus1Scene", "DialogueWhiterunHrongarProventus2Scene", "DialogueWhiterunIrilethBalgruuf1Scene",
                "DialogueWhiterunIrilethProventus1Scene", "DialogueWhiterunNazeemYsoldaScene1View", "DialogueWhiterunOlfinaYsoldaScene1View", "DialogueWhiterunProventusBalgruuf1Scene", "DialogueWhiterunSceneVignarBalgruuf1",
                "DialogueWhiterunTempleCastOnSoldier", "DialogueWhiterunTempleFarmerScene", "DialogueWhiterunTempleHealFarmerScene", "DialogueWhiterunTempleHealSoldierScene", "DialogueWhiterunTempleSoldierScene",
                "DialogueWhiterunTempleTalkFarmerScene", "DialogueWhiterunTempleTalkSoldierScene", "DialogueWhiterunYsoldaBrenuinScene1Scene", "DialogueWindhelmCandlehearthEldaCalixtoScene7", "DialogueWindhelmCandlehearthEldaLonelyGaleScene6",
                "DialogueWindhelmCandlehearthHallScene1", "DialogueWindhelmCandlehearthHallScene2", "DialogueWindhelmCandlehearthHallScene3", "DialogueWindhelmEldaLonelyGaleScene1", "DialogueWindhelmMarketSceneBrunwulfAvalScene",
                "DialogueWindhelmMarketSceneJovaNiranyeScene", "DialogueWindhelmMarketSceneLonelyGaleNiranyeScene", "DialogueWindhelmMarketSceneNilsineHilleviScene", "DialogueWindhelmMarketSceneTorbjornHilleviScene", "DialogueWindhelmMarketSceneTorbjornNiranyeScene",
                "DialogueWindhelmMarketSceneTorstenHilleviScene", "DialogueWindhelmMarketSceneTorstenNiranyeScene", "DialogueWindhelmMarketSceneTovaAvalScene", "DialogueWindhelmNurelionQuintusScene1", "DialogueWindhelmNurelionQuintusScene2",
                "DialogueWindhelmNurelionQuintusScene3", "DialogueWindhelmPalaceUlfricGormlaithScene9", "DialogueWindhelmRevynNiranyeScene1", "DialogueWindhelmUlfricGormlaithScene1", "DialogueWindhelmUlfricTorstenScene1",
                "DialogueWindhelmViolaBothersLonelyGaleScene1", "DialogueWindhelmViolaBothersLonelyGaleScene2", "DialogueWindhelmViolaBothersLonelyGaleScene3", "DialogueWindhelmViolaBothersLonelyGaleScene4", "DialogueWinterholdBirnasHouseScene1",
                "DialogueWinterholdBirnasHouseScene2", "DialogueWinterholdInitScene", "DialogueWinterholdInnInitialScene", "DLC1HunterBaseScene1", "DLC1HunterBaseScene10",
                "DLC1HunterBaseScene2", "DLC1HunterBaseScene3", "DLC1HunterBaseScene4", "DLC1HunterBaseScene5", "DLC1HunterBaseScene6",
                "DLC1HunterBaseScene7", "DLC1HunterBaseScene8", "DLC1HunterBaseScene9", "DLC1VampireBaseScene01", "DLC1VampireBaseScene02",
                "DLC1VampireBaseScene03", "DLC1VampireBaseScene04", "DLC1VampireBaseScene05", "DLC1VampireBaseScene06", "DLC2DrovasElyneaScene01",
                "DLC2DrovasElyneaScene02", "DLC2DrovasNelothScene01", "DLC2DrovasNelothScene02", "DLC2DrovasTalvasScene01", "DLC2DrovasTalvasScene02",
                "DLC2MHBujoldElmusReclaimScene01Scene", "DLC2MHBujoldElmusScene01Scene", "DLC2MHBujoldHilundReclaimScene01Scene", "DLC2MHBujoldHilundScene01", "DLC2MHBujoldKuvarReclaimScene01Scene",
                "DLC2MHBujoldKuvarReclaimScene02Scene", "DLC2MHBujoldKuvarReclaimScene03Scene", "DLC2MHBujoldKuvarScene01Scene", "DLC2MHBujoldKuvarScene02Scene", "DLC2MHBujoldKuvarScene03Scene",
                "DLC2MHElmusKuvarReclaimScene01Scene", "DLC2MHElmusKuvarScene01Scene", "DLC2MHElmusKuvarScene02Scene", "DLC2MHKuvarHilundReclaimScene01SCene", "DLC2MHKuvarHilundScene01Scene",
                "DLC2MQ02FreaTempleScene", "DLC2RRAlorHouseScene001", "DLC2RRAlorHouseScene002", "DLC2RRAnyLocScene001", "DLC2RRAnyLocScene0016",
                "DLC2RRAnyLocScene002", "DLC2RRAnyLocScene003", "DLC2RRAnyLocScene004", "DLC2RRAnyLocScene005", "DLC2RRAnyLocScene006",
                "DLC2RRAnyLocScene007", "DLC2RRAnyLocScene008", "DLC2RRAnyLocScene009", "DLC2RRAnyLocScene010", "DLC2RRAnyLocScene011",
                "DLC2RRAnyLocScene012", "DLC2RRAnyLocScene013", "DLC2RRAnyLocScene014", "DLC2RRAnyLocScene015", "DLC2RRAnyLocScene017",
                "DLC2RRAnyLocScene018", "DLC2RRAnyLocScene019", "DLC2RRAnyLocScene020", "DLC2RRAnyLocScene021", "DLC2RRArrivalGoScene",
                "DLC2RRBeggarBralsaScene01", "DLC2RRCresciusHouseScene001", "DLC2RRCresciusHouseScene002", "DLC2RREstablishingScene00", "DLC2RRIenthFarmScene002",
                "DLC2RRMorvaynManorScene001", "DLC2RRMorvaynManorScene002", "DLC2RRMorvaynManorScene003", "DLC2RRMorvaynManorScene004", "DLC2RRMorvaynManorScene005",
                "DLC2RRMorvaynManorScene006", "DLC2RRNetchScene001", "DLC2RRNetchScene002", "DLC2RRNetchScene003", "DLC2RRNetchScene004",
                "DLC2RRNetchScene005", "DLC2RRNetchScene006", "DLC2RRNetchScene007", "DLC2RRNetchScene008", "DLC2RRNetchScene009",
                "DLC2RRNetchScene010", "DLC2RROthrelothPreachingScene00", "DLC2RRSeverinManorScene001", "DLC2RRSeverinManorScene002", "DLC2RRSeverinManorScene003",
                "DLC2RRTempleScene001", "DLC2RRTempleScene002", "DLC2RRTempleScene003", "DLC2SVBaldorMorwenScene01Scene", "DLC2SVBaldorMorwenScene02Scene",
                "DLC2SVBaldorMorwenScene03Scene", "DLC2SVDeorWulfScene01Scene", "DLC2SVDeorWulfScene02Scene", "DLC2SVDeorWulfScene03Scene", "DLC2SVDeorYrsaScene01Scene",
                "DLC2SVDeorYrsaScene02Scene", "DLC2SVEdlaNikulasScene01Scene", "DLC2SVMorwenTharstanScene01Scene", "DLC2SVOslafAetaScene01Scene", "DLC2SVOslafFinnaScene01Scene",
                "DLC2SVOslafFinnaScene02Scene", "DLC2SVOslafYrsaScene01Scene", "DLC2SVTharstanFanariScene01Scene", "DLC2SVTharstanFanariScene02Scene", "DLC2TelMithrynNelothTalvas01",
                "DLC2TelMithrynNelothTalvas02", "DLC2TelMithrynNelothTalvas03", "DLC2TelMithrynNelothTalvas04", "DLC2VaronaElyneaScene01", "DLC2VaronaElyneaScene02",
                "DLC2VaronaNelothScene01", "DLC2VaronaNelothScene02", "DLC2VaronaTalvasScene01", "DLC2VaronaTalvasScene02", "DLC2VaronaUlvesScene01",
                "DLC2VaronaUlvesScene02", "DragonBridgeFarmScene01", "DragonBridgeFarmScene02", "DragonBridgeHorgeirsHouseScene01", "DragonBridgeMillSceneHorgeirLodvar",
                "DragonBridgeMillSceneLodvarOlda", "DragonBridgeScene01", "DragonBridgeTavernSceneFaidaJuli", "DragonBridgeTavernSceneJuliFaida",
                "DrinkingContest", "DryGoodsCamillaLucanScene", "EasternMineScene01", "EasternMineScene02", "EndonKerahMarketScene1",
                "EndonKerahMarketScene3", "ErikurFamilyScene", "ErikurFamilyScene2", "ErikurMelaranScene", "FalkElisifAboutPower",
                "FalkreathCemeteryScene01", "FalkreathCemeteryScene02", "FalkreathCorpselightFarmScene01", "FalkreathDeadMansDrinkScene01", "FalkreathDeadMansDrinkScene02",
                "FalkreathDeadMansDrinkScene03", "FalkreathDeadMansDrinkScene04", "FalkreathDeadMansDrinkScene05", "FalkreathDeadMansDrinkScene06", "FalkreathDeadMansDrinkScene07",
                "FalkreathDeadMansDrinkScene08", "FalkreathDeadMansDrinkScene09", "FalkreathDeadMansDrinkScene10", "FalkreathDeadMansDrinkScene11", "FalkreathDengeirsHallScene01",
                "FalkreathDengeirsHallScene02", "FalkreathDengeirsHallScene03", "FalkreathGrayPineGoodsScene01", "FalkreathHouseofArkayScene01", "FalkreathHouseofArkayScene02",
                "FalkreathHouseofArkayScene03", "FalkreathJarlsLonghouseScene01", "FalkreathJarlsLonghouseScene02", "FalkreathJarlsLonghouseScene03", "FalkSybille",
                "FaraldaEnthirScene01", "FletcherScene", "FletcherScene2", "GretaLookingForWork", "HeartwoodMillScene01",
                "HeartwoodMillScene02", "HighmoonHallScene01", "HighmoonHallScene02", "HighmoonHallScene03", "HighmoonHallScene04",
                "HighmoonHallScene05", "HighmoonHallScene06", "HighmoonHallScene07", "IvarsteadSSScene", "JailerScene2",
                "JailScene1", "JornAiaScene", "JzargoOnmundDormScene01", "KatlaFamilyScene", "KidsPlaying",
                "LoreiusFarmOutsideScene01", "LoreiusFarmOutsideScene02",
                "MarkarthArnleifandsonsScene01", "MarkarthArnleifandSonsScene02", "MarkarthEndonsHouseScene01", "MarkarthEndonsHouseScene02", "MarkarthHagsCureScene01",
                "MarkarthHagsCureScene02", "MarkarthKeepScene01", "MarkarthKeepScene02", "MarkarthKeepScene03", "MarkarthKeepScene04",
                "MarkarthKeepScene05", "MarkarthKeepScene06", "MarkarthKeepScene08", "MarkarthKeepScene10", "MarkarthKeepScene10DUPLICATE001",
                "MarkarthSilverFishhInnScene13", "MarkarthSilverFishInnScene01", "MarkarthSilverFishInnScene02", "MarkarthSilverFishInnScene03", "MarkarthSilverFishInnScene04",
                "MarkarthSilverFishInnScene05", "MarkarthSilverFishInnScene06", "MarkarthSilverFishInnScene12", "MarkarthSilverFishInnScene14", "MarkarthTreasuryHouseScene1",
                "MarkarthTreasuryHouseScene10", "MarkarthTreasuryHouseScene2", "MarkarthTreasuryHouseScene3", "MarkarthTreasuryHouseScene6", "MarkarthTreasuryHouseScene7",
                "MarkarthTreasuryHouseScene8", "MarkarthWarrensScene01", "MarkarthWarrensScene03", "MarkarthWarrensScene04", "MarkarthWizardsTowerScene01",
                "MarkarthWizardsTowerScene02", "MerryfairFarmScene02", "MerryfairScene01", "MjollsHouseScene01",
                "MoorsideScene05", "MoorsideScene06", "MorthalAlvasHouseScene1", "MorthalFalionsHouseScene1", "MorthalFalionsHouseScene2",
                "MorthalFalionsHouseScene3", "MorthalFalionsHouseScene4", "MorthalMoorsideScene07", "MorthalMoorsideScene08",
                "MorthalMoorsideScene1", "MorthalMoorsideScene2", "MorthalMoorsideScene3", "MorthalMoorsideScene4", "MorthalThaumaturgistHutScene01",
                "MorthalThaumaturgistsHutScene2", "MorthalThaumaturgistsHutScene3", "NosterBegging", "PalaceScene1", "RaimentsScene",
                "RDS01Scene", "RDSScene04", "RDSScene05", "RiftenBBManorScene01", "RiftenBBManorScene02",
                "RiftenBBMeaderyScene01", "RiftenBBMeaderyScene02", "RiftenBBMeaderyScene03", "RiftenBeeAndBarbScene01", "RiftenBeeAndBarbScene02",
                "RiftenBeeAndBarbScene03", "RiftenBeeAndBarbScene04", "RiftenBeeAndBarbScene05", "RiftenBeeAndBarbScene06", "RiftenBeeAndBarbScene07",
                "RiftenBeeAndBarbScene08", "RiftenBeeAndBarbScene09", "RiftenBeeAndBarbScene10", "RiftenBeeAndBarbScene11", "RiftenBeeAndBarbScene12",
                "RiftenBeeandBarbScene13", "RiftenBeeAndBarbScene14", "RiftenBeeAndBarbScene15", "RiftenBeeAndBarbScene16", "RiftenBeeAndBarbScene17",
                "RiftenBeeAndBarbScene18", "RiftenBeeAndBarbScene19", "RiftenBeeandBarbScene20", "RiftenBeggarEddaScene", "RiftenBeggarSnilfScene",
                "RiftenElgrimsElixirsScene01", "RiftenElgrimsElixirsScene02", "RiftenElgrimsElixirsScene03", "RiftenElgrimsElixirsScene4", "RiftenFisheryScene01",
                "RiftenFisheryScene02", "RiftenFisheryScene03", "RiftenFisheryScene04", "RiftenGrandPlazaScene01", "RiftenGrandPlazaScene02",
                "RiftenGrandPlazaScene03", "RiftenGrandPlazaScene04", "RiftenGrandPlazaScene05", "RiftenGrandPlazaScene06", "RiftenGrandPlazaScene07",
                "RiftenGrandPlazaScene08", "RiftenGrandPlazaScene09", "RiftenGrandPlazaScene10", "RiftenGrandPlazaScene11", "RiftenGrandPlazaScene12",
                "RiftenGrandPlazaScene13", "RiftenGrandPlazaScene14", "RiftenGrandPlazaScene15", "RiftenGrandPlazaScene16", "RiftenGrandPlazaScene17",
                "RiftenGrandPlazaScene18", "RiftenGrandPlazaScene20", "RiftenGrandPlazaScene21", "RiftenGrandPlazaScene22", "RiftenGrandPlazaScene23",
                "RiftenGrandPlazaScene24", "RiftenGrandPlazaScene25", "RiftenGrandPlazaScene26", "RiftenGrandPlazaScene27", "RiftenGrandPlazaScene28",
                "RiftenGrandPlazaScene29", "RiftenGrandPlazaScene30", "RiftenGrandPlazaScene31", "RiftenGrandPlazaScene32", "RiftenGrandPlazaScene33",
                "RiftenGrandPlazaScene34", "RiftenGrandPlazaScene35", "RiftenGrandPlazaScene36", "RiftenGrandPlazaScene37", "RiftenGrandPlazaScene38",
                "RiftenGrandPlazaScene39", "RiftenGrandPlazaScene40", "RiftenGrandPlazaScene41", "RiftenGrandPlazaScene42", "RiftenGrandPlazaScene43",
                "RiftenHaelgasBunkhouseScene01", "RiftenHaelgasBunkhouseScene02", "RiftenHaelgasBunkhouseScene03", "RiftenHaelgasBunkhouseScene04", "RiftenHaelgasBunkhouseScene05",
                "RiftenHaelgasBunkhouseScene06", "RiftenHaelgasBunkhouseScene07", "RiftenHaelgasBunkhouseScene08", "RiftenHaelgasBunkhouseScene09", "RiftenHaelgasBunkhouseScene10",
                "RiftenHaelgasBunkhouseScene11", "RiftenHaelgasBunkhouseScene12", "RiftenHonorhallScene01", "RiftenKeepScene01",
                "RiftenKeepScene01Alternate", "RiftenKeepScene02", "RiftenKeepScene02Alternate", "RiftenKeepScene03", "RiftenKeepScene03Alternate",
                "RiftenKeepScene04", "RiftenKeepScene04Alternate", "RiftenKeepScene05", "RiftenKeepScene05Alternate", "RiftenKeepScene06",
                "RiftenKeepScene06Alternate", "RiftenKeepScene07", "RiftenKeepScene07Alternate", "RiftenKeepScene08", "RiftenKeepScene08Alternate01",
                "RiftenKeepScene09", "RiftenKeepScene10", "RiftenKeepScene11", "RiftenMjollHouseScene02", "RiftenPawnedPrawnScene01",
                "RiftenPawnedPrawnScene02", "RiftenPawnedPrawnScene03", "RiftenRaggedFlagon05Scene", "RiftenRaggedFlagonScene01", "RiftenRaggedFlagonScene02",
                "RiftenRaggedFlagonScene03", "RiftenRaggedFlagonScene04", "RiftenRaggedFlagonScene06", "RiftenRaggedFlagonScene07", "RiftenRaggedFlagonScene08",
                "RiftenRaggedFlagonScene09", "RiftenRaggedFlagonScene10", "RiftenRaggedFlagonScene11", "RiftenRaggedFlagonScene12", "RiftenSnowShodHouseScene01",
                "RiftenSnowShodHouseScene02", "RiftenSnowShodHouseScene03", "RiftenTempleofMaraScene01", "RiftenTempleofMaraScene02", "RiverwoodAlvorDortheScene1",
                "RiverwoodAlvorDortheScene2", "RiverwoodDortheFrodnarPlayScene", "RiverwoodFrodnarHodScene1", "RiverwoodIntroSceneHildeSoldierScene", "RiverwoodSceneA",
                "RiverwoodSceneEmbrySoldierScene", "RiverwoodSceneGerdurHod", "RiverwoodSceneSigridAlvorCabbage", "RiverwoodSceneSoldierHildeScene", "RiverwoodSigridDortheScene1",
                "RiverwoodSleepingGiantOrderDrinks", "RiverwoodSleepingGiantScene3", "RiverwoodSleepingGiantScene4", "RiverwoodSleepingGiantScene5", "RiverwoodSleepingGiantScene6",
                "Romance", "RoriksteadBritteSisselScene1", "RoriksteadEnnisReldithScene1", "RoriksteadSisselRouaneScene1", "RustleifSmithyScene01",
                "RustleifSmithyScene02", "SalviusFarmSceneAView", "SanHouse2", "SanHouseScene1", "SarethiFarmScene01",
                "SarethiFarmScene02", "SarethiFarmScene03", "SavosMirabelleScene01", "SavosMirabelleScene02", "SavosMirabelleScene03",
                "SavosMirabelleScene04", "SkyHavenTempleConversation02", "SkyHavenTempleConversation03", "SkyHavenTempleConversation04", "SkyHavenTempleConversationScene",
                "SleepingGiantDelphineOrgnarScene", "SleepingGiantDelphineRaevildScene", "SleepingGiantScene7", "SleepingGiantScene8", "SnowShodFarmScene01",
                "SnowShodFarmScene02", "SolitudeBardScene4", "SolitudeBardScene5", "SolitudeBardScene6", "SolitudeBardsCollegeClassGiraudScene1",
                "SolitudeBardsCollegeClassGiraudScene2", "SolitudeBardsCollegeClassIngeScene1", "SolitudeBardsCollegeClassIngeScene2", "SolitudeBardsCollegeClassPanteaScene1", "SolitudeBardsCollegeClassPanteaScene2",
                "SolitudeMarketplaceAiaEvetteScene1", "SolitudeMarketplaceAtAfAlanAdvarScene1", "SolitudeMarketplaceAtAfAlanEvetteScene1", "SolitudeMarketplaceAtAfAlanJalaScene1", "SolitudeMarketplaceIlldiAdvarScene1",
                "SolitudeMarketplaceIlldiEvetteScene1", "SolitudeMarketplaceIlldiJalaScene1", "SolitudeMarketplaceJawananAdvarScene1", "SolitudeMarketplaceJawananEvetteScene1", "SolitudeMarketplaceJawananJalaScene",
                "SolitudeMarketplaceJornAdvarScene1", "SolitudeMarketplaceJornJalaScene1", "SolitudeMarketplaceRorlundAdvarScene1", "SolitudeMarketplaceRorlundEvetteScene1", "SolitudeMarketplaceRorlundJalaScene1",
                "SolitudeMarketplaceSilanaAdvarScene1", "SolitudeMarketplaceSilanaEvetteScene1", "SolitudeMarketplaceSilanaJalaScene1", "SolitudeMarketplaceSorexAdvarScene1", "SolitudeMarketplaceSorexEvetteScene1",
                "SolitudeMarketplaceSorexJalaScene1", "SolitudeMarketplaceXanderAngelineScene1", "SolitudeMarketplaceXanderSaymaScene1", "SolitudeMarketplaceXanderTaarieScene1", "SolitudeSawmillScene",
                "SolitudeStreetOdarJawananScene1", "SolitudeStreetOdarJornScene1", "SolitudeStreetUnaJornScene1", "SolitudeStreetUnaLisetteScene1", "SolitudeStreetVivienneLisetteScene",
                "SolitudeStreetVivienneLisetteScene2", "SolitudeStreetVivienneLisetteScene3", "StonehillsGesturJesperScene1", "StonehillsGesturSwanhvirScene1", "StonehillsGesturTeebaEiScene1",
                "StonehillsMineGesturTalibScene1", "StudentTeacher", "TeachersScene", "TempleScene1", "TempleScene2",
                "TempleScene3", "UragColetteScene01", "UragColetteScene02", "UragDrevisScene02", "UragNiryaScene01",
                "UragNiryaScene02", "UragSergiusScene01", "ViniusFamilyScene", "ViniusFamilyScene2", "WarehouseWorkScene",
                "WesternMineScene01", "WesternMineScene02", "WhiteHallImperialScene01", "WhiteHallImperialScene02",
                "WhiteHallImperialScene03", "WhiteHallImperialScene04", "WhiteHallImperialScene05", "WhiteHallSonsScene01", "WhiteHallSonsScene02",
                "WhiteHallSonsScene03", "WhiteHallSonsScene04", "WhiteHallSonsScene05", "WhiteHallsonsScene06",
                "WhiterunAnoriathElrindirScene1", "WhiterunAnoriathElrindirScene2", "WhiterunAnoriathElrindirScene3",
                "WhiterunBanneredMareScene1", "WhiterunBanneredMareScene2", "WhiterunBanneredMareScene3", "WhiterunBraithAmrenScene1", "WhiterunBraithAmrenScene2",
                "WhiterunBraithLarsScene1", "WhiterunBraithLarsScene2", "WhiterunBraithLarsScene3", "WhiterunBraithSaffirScene1", "WhiterunBraithSaffirScene2",
                "WhiterunCivilWarArgueScene", "WhiterunDragonsreachScene1",
                "WhiterunHeimskrPreachScene", "WhiterunHouseBattleBornScene1", "WhiterunHouseBattleBornScene2", "WhiterunHouseGrayManeScene1", "WhiterunHouseGrayManeScene2",
                "WhiterunMarketplaceScene1", "WhiterunMarketplaceScene2", "WhiterunMarketplaceScene3", "WhiterunMarketplaceScene4a",
                "WhiterunOlfinaJonScene1", "WhiterunOlfinaJonScene2", "WhiterunOlfinaJonScene3",
                "WhiterunStablesScene1", "WhiterunStablesScene2", "WhiterunStablesScene3", "WhiterunStablesScene4", "WhiterunTempleOfKynarethScene1",
                "WhiterunVignarBrillScene1", "WhiterunVignarBrillScene2", "WhiterunVignarBrillScene3", "WIGreetingScene",
                "WindhelmDialogueAmbaryScoutsManyMarshesScene1", "WindhelmDialogueAmbarysScoutsManyMarshesScene2", "WindhelmDialogueAmbarySuvarisScene1", "WindhelmDialogueAmbarySuvarisScene2",
                "WindhelmDialogueEldaCalixtoScene1", "WindhelmDialogueEldaJoraScene1", "WindhelmDialogueEldaJoraScene2", "WindhelmDialogueUlfricGormlaith2", "WindhelmDialogueUlfricJorleifScene1",
                "WindhelmDialogueUlfricJorleifScene2", "WindhelmDialogueUlfricLonelyGaleScene1", "WindhelmDialogueUlfricLonelyGaleScene2",
                "WindhelmOengulHermirScene1", "WindhelmOengulHermirScene2", "WindhelmOengulHermirScene3", "WindhelmUlfricJorleifScene3", "WindpeakInnScene02",
                "WindpeakInnScene03", "WinterholdInnScene01", "WinterholdInnScene02", "WinterholdInnScene03", "WinterholdInnScene04",
                "WinterholdInnScene05", "WinterholdInnScene06", "WinterholdInnScene07", "WinterholdInnScene08", "WinterholdJarlsLonghouseScene01",
                "WinterholdJarlsLonghouseScene02", "WinterholdJarlsLonghouseScene03", "WinterholdKorirsHouseScene01", "WinterholdKorirsHouseScene02", "WinterholdKorirsHouseScene03",
                "WIRentRoomWalkToScene", "WITavernGreeting", "WITavernPlayerSits"
            };
            
            // Look for scene blacklist in STFU Patcher/Config (relative to executable)
            var blacklistPath = Path.Combine(executableDir, "Config", "STFU_Blacklist.yaml");
            var blacklistedTopics = new HashSet<FormKey>();
            var blacklistedTopicEditorIds = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            var blacklistedPlugins = new HashSet<string>(StringComparer.OrdinalIgnoreCase); // Blacklist all topics from specific plugins
            var blacklistedSceneEditorIds = new HashSet<string>(StringComparer.OrdinalIgnoreCase); // Track scene EditorIDs for patching
            var blacklistedQuestEditorIds = new HashSet<string>(StringComparer.OrdinalIgnoreCase); // Track quest EditorIDs for complete blocking
            var blacklistedQuestFormKeys = new HashSet<FormKey>(); // Track quest FormKeys for complete blocking
            
            if (File.Exists(blacklistPath))
            {
                Console.WriteLine($"STFU: Loading scene blacklist from {blacklistPath}");
                try
                {
                    var yamlText = File.ReadAllText(blacklistPath);
                    var deserializer = new DeserializerBuilder()
                        .WithNamingConvention(CamelCaseNamingConvention.Instance)
                        .Build();
                    var config = deserializer.Deserialize<Dictionary<string, object>>(yamlText);
                    
                    if (config != null)
                    {
                        // Load individual topics
                        if (config.TryGetValue("topics", out var topicsObj) && topicsObj is List<object> topicsList)
                        {
                            foreach (var topicObj in topicsList)
                            {
                                var topicStr = topicObj?.ToString()?.Trim();
                                if (string.IsNullOrWhiteSpace(topicStr)) continue;
                                
                                try
                                {
                                    // Check if it's a FormKey (contains ':') or Editor ID
                                    if (topicStr.Contains(":"))
                                    {
                                        // Remove 0x prefix if present
                                        var formKeyString = topicStr.StartsWith("0x", StringComparison.OrdinalIgnoreCase) 
                                            ? topicStr.Substring(2) 
                                            : topicStr;
                                        
                                        // Pad FormID to 6 digits
                                        var parts = formKeyString.Split(':');
                                        if (parts.Length == 2)
                                        {
                                            var formID = parts[0].PadLeft(6, '0');
                                            formKeyString = $"{formID}:{parts[1]}";
                                        }
                                        
                                        if (FormKey.TryFactory(formKeyString, out var formKey))
                                        {
                                            blacklistedTopics.Add(formKey);
                                        }
                                        else
                                        {
                                            Console.WriteLine($"Warning: Invalid FormKey in blacklist: {topicStr}");
                                        }
                                    }
                                    else
                                    {
                                        // It's an Editor ID
                                        blacklistedTopicEditorIds.Add(topicStr);
                                    }
                                }
                                catch (Exception ex)
                                {
                                    Console.WriteLine($"Warning: Error parsing blacklist entry '{topicStr}': {ex.Message}");
                                }
                            }
                        }
                        
                        // Load blacklisted plugins (all topics from these plugins will be blocked)
                        if (config.TryGetValue("plugins", out var pluginsObj) && pluginsObj is List<object> pluginsList)
                        {
                            foreach (var pluginObj in pluginsList)
                            {
                                var pluginName = pluginObj?.ToString()?.Trim();
                                if (!string.IsNullOrWhiteSpace(pluginName))
                                {
                                    blacklistedPlugins.Add(pluginName);
                                    Console.WriteLine($"STFU: Blacklisted plugin '{pluginName}'");
                                }
                            }
                        }
                        
                        // Load scenes and extract their topics
                        if (config.TryGetValue("scenes", out var scenesObj) && scenesObj is List<object> scenesList)
                        {
                            foreach (var sceneObj in scenesList)
                            {
                                var sceneStr = sceneObj?.ToString()?.Trim();
                                if (string.IsNullOrWhiteSpace(sceneStr)) continue;
                                
                                try
                                {
                                    ISceneGetter? scene = null;
                                    
                                    // Check if it's a FormKey (contains ':') or Editor ID
                                    if (sceneStr.Contains(":"))
                                    {
                                        // Remove 0x prefix if present
                                        var formKeyString = sceneStr.StartsWith("0x", StringComparison.OrdinalIgnoreCase) 
                                            ? sceneStr.Substring(2) 
                                            : sceneStr;
                                        
                                        // Pad FormID to 6 digits
                                        var parts = formKeyString.Split(':');
                                        if (parts.Length == 2)
                                        {
                                            var formID = parts[0].PadLeft(6, '0');
                                            formKeyString = $"{formID}:{parts[1]}";
                                        }
                                        
                                        if (FormKey.TryFactory(formKeyString, out var sceneFormKey))
                                        {
                                            scene = state.LoadOrder.PriorityOrder.Scene().WinningOverrides()
                                                .FirstOrDefault(s => s.FormKey == sceneFormKey);
                                        }
                                    }
                                    else
                                    {
                                        // It's an Editor ID
                                        scene = state.LoadOrder.PriorityOrder.Scene().WinningOverrides()
                                            .FirstOrDefault(s => s.EditorID?.Equals(sceneStr, StringComparison.OrdinalIgnoreCase) == true);
                                    }
                                    
                                    if (scene != null)
                                    {
                                        // Store scene EditorID for later patching
                                        if (!string.IsNullOrEmpty(scene.EditorID))
                                        {
                                            blacklistedSceneEditorIds.Add(scene.EditorID);
                                        }
                                        Console.WriteLine($"STFU: Blacklisted scene '{sceneStr}'");
                                    }
                                    else
                                    {
                                        Console.WriteLine($"Warning: Scene not found in blacklist: {sceneStr}");
                                    }
                                }
                                catch (Exception ex)
                                {
                                    Console.WriteLine($"Warning: Error processing blacklisted scene '{sceneStr}': {ex.Message}");
                                }
                            }
                        }
                        
                        // Load quests and add blocking
                        if (config.TryGetValue("quests", out var questsObj) && questsObj is List<object> questsList)
                        {
                            foreach (var questObj in questsList)
                            {
                                var questStr = questObj?.ToString()?.Trim();
                                if (string.IsNullOrWhiteSpace(questStr)) continue;
                                
                                try
                                {
                                    // Check if it's a FormKey (contains ':') or Editor ID
                                    if (questStr.Contains(":"))
                                    {
                                        // Remove 0x prefix if present
                                        var formKeyString = questStr.StartsWith("0x", StringComparison.OrdinalIgnoreCase) 
                                            ? questStr.Substring(2) 
                                            : questStr;
                                        
                                        // Pad FormID to 6 digits
                                        var parts = formKeyString.Split(':');
                                        if (parts.Length == 2)
                                        {
                                            var formID = parts[0].PadLeft(6, '0');
                                            formKeyString = $"{formID}:{parts[1]}";
                                        }
                                        
                                        if (FormKey.TryFactory(formKeyString, out var formKey))
                                        {
                                            blacklistedQuestFormKeys.Add(formKey);
                                            Console.WriteLine($"STFU: Blacklisted quest (FormKey) '{questStr}'");
                                        }
                                        else
                                        {
                                            Console.WriteLine($"Warning: Invalid FormKey in quest blacklist: {questStr}");
                                        }
                                    }
                                    else
                                    {
                                        // It's an Editor ID
                                        blacklistedQuestEditorIds.Add(questStr);
                                        Console.WriteLine($"STFU: Blacklisted quest '{questStr}'");
                                    }
                                }
                                catch (Exception ex)
                                {
                                    Console.WriteLine($"Warning: Error parsing blacklist quest entry '{questStr}': {ex.Message}");
                                }
                            }
                        }
                        
                        // Load quest patterns and match against all quests
                        if (config.TryGetValue("quest_patterns", out var questPatternsObj) && questPatternsObj is List<object> questPatternsList)
                        {
                            Console.WriteLine($"STFU: Processing {questPatternsList.Count} quest patterns...");
                            
                            foreach (var patternObj in questPatternsList)
                            {
                                var pattern = patternObj?.ToString()?.Trim();
                                if (string.IsNullOrWhiteSpace(pattern)) continue;
                                
                                // Convert wildcard pattern to regex (* = any chars, ? = single char)
                                var regexPattern = "^" + System.Text.RegularExpressions.Regex.Escape(pattern)
                                    .Replace("\\*", ".*")
                                    .Replace("\\?", ".") + "$";
                                var regex = new System.Text.RegularExpressions.Regex(regexPattern, System.Text.RegularExpressions.RegexOptions.IgnoreCase);
                                
                                int matchCount = 0;
                                foreach (var quest in state.LoadOrder.PriorityOrder.Quest().WinningOverrides())
                                {
                                    if (!string.IsNullOrEmpty(quest.EditorID) && regex.IsMatch(quest.EditorID))
                                    {
                                        // Check whitelist before adding (both EditorID and FormKey)
                                        if (!whitelistedQuestEditorIds.Contains(quest.EditorID) && !whitelistedQuestFormKeys.Contains(quest.FormKey))
                                        {
                                            if (blacklistedQuestEditorIds.Add(quest.EditorID))
                                            {
                                                matchCount++;
                                            }
                                        }
                                    }
                                }
                                Console.WriteLine($"  Pattern '{pattern}': matched {matchCount} quests");
                            }
                        }
                    }
                    Console.WriteLine($"STFU: Loaded {blacklistedTopics.Count} blacklisted FormKeys, {blacklistedTopicEditorIds.Count} blacklisted Editor IDs, {blacklistedPlugins.Count} blacklisted plugins, {blacklistedQuestEditorIds.Count} blacklisted quests, and {blacklistedQuestFormKeys.Count} blacklisted quest FormKeys");
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"Warning: Error loading blacklist YAML: {ex.Message}");
                }
            }
            
            Console.WriteLine($"STFU: Loaded {hardcodedSceneEditorIds.Count} hardcoded ambient scene EditorIDs");
            
            // Look for configuration file in STFU Patcher/Config
            var configPath = Path.Combine(executableDir, "Config", "STFU_Config.ini");
            var vanillaOnly = false;
            var safeMode = false; // If true, skip patching responses with scripts attached
            var disabledSubtypes = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            var hasFilterConfig = false; // Track if we found ANY filter settings
            var vanillaPlugins = new HashSet<string>(StringComparer.OrdinalIgnoreCase)
            {
                "Skyrim.esm", "Update.esm", "Dawnguard.esm", "HearthFires.esm", "Dragonborn.esm"
            };
            
            if (File.Exists(configPath))
            {
                Console.WriteLine($"STFU: Loading configuration from {configPath}");
                try
                {
                    var iniConfig = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
                    var lines = File.ReadAllLines(configPath);
                    
                    // Simple INI parser
                    foreach (var line in lines)
                    {
                        var trimmed = line.Trim();
                        
                        // Skip comments and empty lines
                        if (string.IsNullOrEmpty(trimmed) || trimmed.StartsWith(";") || trimmed.StartsWith("#") || trimmed.StartsWith("["))
                            continue;
                        
                        var parts = trimmed.Split(new[] { '=' }, 2);
                        if (parts.Length == 2)
                        {
                            var key = parts[0].Trim();
                            var value = parts[1].Trim();
                            iniConfig[key] = value;
                        }
                    }
                    
                    // Load VanillaOnly setting
                    if (iniConfig.TryGetValue("vanillaOnly", out var vanillaOnlyStr))
                    {
                        vanillaOnly = vanillaOnlyStr.Equals("true", StringComparison.OrdinalIgnoreCase);
                        Console.WriteLine($"STFU: VanillaOnly = {vanillaOnly}");
                    }
                    
                    // Load SafeMode setting
                    if (iniConfig.TryGetValue("safeMode", out var safeModeStr))
                    {
                        safeMode = safeModeStr.Equals("true", StringComparison.OrdinalIgnoreCase);
                        Console.WriteLine($"STFU: SafeMode = {safeMode}");
                    }
                    
                    // Load per-subtype enable/disable flags
                    // If filter settings exist: only explicitly true subtypes are enabled
                    // If no filter settings exist: all subtypes are enabled by default
                    foreach (var kvp in iniConfig)
                    {
                        if (kvp.Key.StartsWith("filter", StringComparison.OrdinalIgnoreCase))
                        {
                            bool enabled = kvp.Value.Equals("true", StringComparison.OrdinalIgnoreCase);
                            
                            hasFilterConfig = true; // Found at least one filter setting
                            
                            // Extract subtype name from "filterHello" -> "Hello"
                            var subtypeName = kvp.Key.Substring(6); // Remove "filter" prefix
                            
                            if (!enabled)
                            {
                                disabledSubtypes.Add(subtypeName);
                                Console.WriteLine($"STFU: Disabled filtering for {subtypeName}");
                            }
                            else
                            {
                                Console.WriteLine($"STFU: Enabled filtering for {subtypeName}");
                            }
                        }
                    }
                    
                    // Report configuration results
                    if (!hasFilterConfig)
                    {
                        Console.WriteLine("STFU: No filter settings found in config, enabling all subtypes by default");
                    }
                    else
                    {
                        Console.WriteLine($"STFU: Config has filter settings - {disabledSubtypes.Count} subtypes disabled, others enabled");
                    }
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"Warning: Error loading config INI: {ex.Message}");
                    Console.WriteLine("STFU: Defaulting to all subtypes enabled");
                }
            }
            else
            {
                Console.WriteLine($"STFU: No config file found at {configPath}, enabling all subtypes by default");
            }
            
            // Look for whitelist in STFU Patcher/Config (topics to NEVER patch)
            var whitelistPath = Path.Combine(executableDir, "Config", "STFU_Whitelist.yaml");
            var whitelistedTopics = new HashSet<FormKey>();
            var whitelistedTopicEditorIds = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            var whitelistedPlugins = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            var whitelistedSceneEditorIds = new HashSet<string>(StringComparer.OrdinalIgnoreCase); // Track scene EditorIDs to never patch
            
            if (File.Exists(whitelistPath))
            {
                Console.WriteLine($"STFU: Loading topic whitelist from {whitelistPath}");
                try
                {
                    var yamlText = File.ReadAllText(whitelistPath);
                    var deserializer = new DeserializerBuilder()
                        .WithNamingConvention(CamelCaseNamingConvention.Instance)
                        .Build();
                    var config = deserializer.Deserialize<Dictionary<string, object>>(yamlText);
                    
                    if (config != null)
                    {
                        // Load individual topics
                        if (config.TryGetValue("topics", out var topicsObj) && topicsObj is List<object> topicsList)
                        {
                            foreach (var topicObj in topicsList)
                            {
                                var topicStr = topicObj?.ToString()?.Trim();
                                if (string.IsNullOrWhiteSpace(topicStr)) continue;
                                
                                try
                                {
                                    // Check if it's a FormKey (contains ':') or Editor ID
                                    if (topicStr.Contains(":"))
                                    {
                                        // Remove 0x prefix if present
                                        var formKeyString = topicStr.StartsWith("0x", StringComparison.OrdinalIgnoreCase) 
                                            ? topicStr.Substring(2) 
                                            : topicStr;
                                        
                                        // Pad FormID to 6 digits
                                        var parts = formKeyString.Split(':');
                                        if (parts.Length == 2)
                                        {
                                            var formID = parts[0].PadLeft(6, '0');
                                            formKeyString = $"{formID}:{parts[1]}";
                                        }
                                        
                                        if (FormKey.TryFactory(formKeyString, out var formKey))
                                        {
                                            whitelistedTopics.Add(formKey);
                                        }
                                        else
                                        {
                                            Console.WriteLine($"Warning: Invalid FormKey in whitelist: {topicStr}");
                                        }
                                    }
                                    else
                                    {
                                        // It's an Editor ID
                                        whitelistedTopicEditorIds.Add(topicStr);
                                    }
                                }
                                catch (Exception ex)
                                {
                                    Console.WriteLine($"Warning: Error parsing whitelist entry '{topicStr}': {ex.Message}");
                                }
                            }
                        }
                        
                        // Load whitelisted scenes (these will NEVER be patched)
                        if (config.TryGetValue("scenes", out var scenesObj) && scenesObj is List<object> scenesList)
                        {
                            foreach (var sceneObj in scenesList)
                            {
                                var sceneStr = sceneObj?.ToString()?.Trim();
                                if (string.IsNullOrWhiteSpace(sceneStr)) continue;
                                
                                try
                                {
                                    ISceneGetter? scene = null;
                                    
                                    // Check if it's a FormKey (contains ':') or Editor ID
                                    if (sceneStr.Contains(":"))
                                    {
                                        // Remove 0x prefix if present
                                        var formKeyString = sceneStr.StartsWith("0x", StringComparison.OrdinalIgnoreCase) 
                                            ? sceneStr.Substring(2) 
                                            : sceneStr;
                                        
                                        // Pad FormID to 6 digits
                                        var parts = formKeyString.Split(':');
                                        if (parts.Length == 2)
                                        {
                                            var formID = parts[0].PadLeft(6, '0');
                                            formKeyString = $"{formID}:{parts[1]}";
                                        }
                                        
                                        if (FormKey.TryFactory(formKeyString, out var sceneFormKey))
                                        {
                                            scene = state.LoadOrder.PriorityOrder.Scene().WinningOverrides()
                                                .FirstOrDefault(s => s.FormKey == sceneFormKey);
                                        }
                                    }
                                    else
                                    {
                                        // It's an Editor ID
                                        scene = state.LoadOrder.PriorityOrder.Scene().WinningOverrides()
                                            .FirstOrDefault(s => s.EditorID?.Equals(sceneStr, StringComparison.OrdinalIgnoreCase) == true);
                                    }
                                    
                                    if (scene != null)
                                    {
                                        // Store scene EditorID for later exclusion from patching
                                        if (!string.IsNullOrEmpty(scene.EditorID))
                                        {
                                            whitelistedSceneEditorIds.Add(scene.EditorID);
                                        }
                                        Console.WriteLine($"STFU: Whitelisted scene '{sceneStr}'");
                                    }
                                    else
                                    {
                                        Console.WriteLine($"Warning: Scene not found in whitelist: {sceneStr}");
                                    }
                                }
                                catch (Exception ex)
                                {
                                    Console.WriteLine($"Warning: Error processing whitelisted scene '{sceneStr}': {ex.Message}");
                                }
                            }
                        }
                        
                        // Load whitelisted plugins
                        if (config.TryGetValue("plugins", out var pluginsObj) && pluginsObj is List<object> pluginsList)
                        {
                            foreach (var pluginObj in pluginsList)
                            {
                                var pluginName = pluginObj?.ToString()?.Trim();
                                if (!string.IsNullOrWhiteSpace(pluginName))
                                {
                                    whitelistedPlugins.Add(pluginName);
                                }
                            }
                        }
                        
                        // Load whitelisted quests (these will NEVER be blocked, even if they match patterns)
                        if (config.TryGetValue("quests", out var questsObj) && questsObj is List<object> questsList)
                        {
                            foreach (var questObj in questsList)
                            {
                                var questStr = questObj?.ToString()?.Trim();
                                if (string.IsNullOrWhiteSpace(questStr)) continue;
                                
                                try
                                {
                                    // Check if it's a FormKey (contains ':') or Editor ID
                                    if (questStr.Contains(":"))
                                    {
                                        // Remove 0x prefix if present
                                        var formKeyString = questStr.StartsWith("0x", StringComparison.OrdinalIgnoreCase) 
                                            ? questStr.Substring(2) 
                                            : questStr;
                                        
                                        // Pad FormID to 6 digits
                                        var parts = formKeyString.Split(':');
                                        if (parts.Length == 2)
                                        {
                                            var formID = parts[0].PadLeft(6, '0');
                                            formKeyString = $"{formID}:{parts[1]}";
                                        }
                                        
                                        if (FormKey.TryFactory(formKeyString, out var formKey))
                                        {
                                            whitelistedQuestFormKeys.Add(formKey);
                                            Console.WriteLine($"STFU: Whitelisted quest (FormKey) '{questStr}'");
                                        }
                                        else
                                        {
                                            Console.WriteLine($"Warning: Invalid FormKey in quest whitelist: {questStr}");
                                        }
                                    }
                                    else
                                    {
                                        // It's an Editor ID
                                        whitelistedQuestEditorIds.Add(questStr);
                                        Console.WriteLine($"STFU: Whitelisted quest '{questStr}'");
                                    }
                                }
                                catch (Exception ex)
                                {
                                    Console.WriteLine($"Warning: Error parsing whitelist quest entry '{questStr}': {ex.Message}");
                                }
                            }
                        }
                        
                        // Load quest patterns and match against all quests (whitelist protection)
                        if (config.TryGetValue("quest_patterns", out var questPatternsObj) && questPatternsObj is List<object> questPatternsList)
                        {
                            Console.WriteLine($"STFU: Processing {questPatternsList.Count} whitelist quest patterns...");
                            
                            foreach (var patternObj in questPatternsList)
                            {
                                var pattern = patternObj?.ToString()?.Trim();
                                if (string.IsNullOrWhiteSpace(pattern)) continue;
                                
                                // Convert wildcard pattern to regex (* = any chars, ? = single char)
                                var regexPattern = "^" + System.Text.RegularExpressions.Regex.Escape(pattern)
                                    .Replace("\\*", ".*")
                                    .Replace("\\?", ".") + "$";
                                var regex = new System.Text.RegularExpressions.Regex(regexPattern, System.Text.RegularExpressions.RegexOptions.IgnoreCase);
                                
                                int matchCount = 0;
                                foreach (var quest in state.LoadOrder.PriorityOrder.Quest().WinningOverrides())
                                {
                                    if (!string.IsNullOrEmpty(quest.EditorID) && regex.IsMatch(quest.EditorID))
                                    {
                                        if (whitelistedQuestEditorIds.Add(quest.EditorID))
                                        {
                                            matchCount++;
                                        }
                                    }
                                }
                                Console.WriteLine($"  Pattern '{pattern}': matched {matchCount} quests");
                            }
                        }
                    }
                    Console.WriteLine($"STFU: Loaded {whitelistedTopics.Count} whitelisted FormKeys, {whitelistedTopicEditorIds.Count} whitelisted Editor IDs, {whitelistedPlugins.Count} whitelisted plugins, {whitelistedSceneEditorIds.Count} whitelisted scenes, {whitelistedQuestEditorIds.Count} whitelisted quests, and {whitelistedQuestFormKeys.Count} whitelisted quest FormKeys (will not be patched)");
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"Warning: Error loading whitelist YAML: {ex.Message}");
                }
            }
            
            // YAML whitelist has final say - remove any blacklisted quests that are in the whitelist
            int whitelistOverrides = 0;
            foreach (var whitelistedQuest in whitelistedQuestEditorIds)
            {
                if (blacklistedQuestEditorIds.Remove(whitelistedQuest))
                {
                    whitelistOverrides++;
                }
            }
            foreach (var whitelistedQuestFormKey in whitelistedQuestFormKeys)
            {
                if (blacklistedQuestFormKeys.Remove(whitelistedQuestFormKey))
                {
                    whitelistOverrides++;
                }
            }
            if (whitelistOverrides > 0)
            {
                Console.WriteLine($"STFU: Whitelist overrode {whitelistOverrides} blacklisted quest(s)");
            }
            
            // Look for subtype overrides file in STFU Patcher/Config
            var subtypeOverridesPath = Path.Combine(executableDir, "Config", "STFU_SubtypeOverrides.yaml");
            var fileBasedFormKeyOverrides = new Dictionary<FormKey, string>();
            var fileBasedEditorIdOverrides = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            
            if (File.Exists(subtypeOverridesPath))
            {
                Console.WriteLine($"STFU: Loading subtype overrides from {subtypeOverridesPath}");
                try
                {
                    var yamlText = File.ReadAllText(subtypeOverridesPath);
                    var deserializer = new DeserializerBuilder()
                        .WithNamingConvention(CamelCaseNamingConvention.Instance)
                        .Build();
                    var config = deserializer.Deserialize<Dictionary<string, Dictionary<string, string>>>(yamlText);
                    
                    if (config != null && config.TryGetValue("overrides", out var overridesDict) && overridesDict != null)
                    {
                        foreach (var kvp in overridesDict)
                        {
                            var key = kvp.Key.Trim();
                            var subtype = kvp.Value?.Trim();
                            
                            if (string.IsNullOrWhiteSpace(key) || string.IsNullOrWhiteSpace(subtype))
                                continue;
                            
                            try
                            {
                                // Check if it's a FormKey (contains ':')
                                if (key.Contains(":"))
                                {
                                    // Remove 0x prefix if present
                                    var formKeyString = key.StartsWith("0x", StringComparison.OrdinalIgnoreCase) 
                                        ? key.Substring(2) 
                                        : key;
                                    
                                    // Pad FormID to 6 digits
                                    var parts = formKeyString.Split(':');
                                    if (parts.Length == 2)
                                    {
                                        var formID = parts[0].PadLeft(6, '0');
                                        formKeyString = $"{formID}:{parts[1]}";
                                    }
                                    
                                    if (FormKey.TryFactory(formKeyString, out var formKey))
                                    {
                                        fileBasedFormKeyOverrides[formKey] = subtype;
                                    }
                                    else
                                    {
                                        Console.WriteLine($"Warning: Invalid FormKey in override: {key}");
                                    }
                                }
                                else
                                {
                                    // It's an Editor ID
                                    fileBasedEditorIdOverrides[key] = subtype;
                                }
                            }
                            catch (Exception ex)
                            {
                                Console.WriteLine($"Warning: Error parsing override '{key}': {ex.Message}");
                            }
                        }
                    }
                    Console.WriteLine($"STFU: Loaded {fileBasedFormKeyOverrides.Count} FormKey overrides and {fileBasedEditorIdOverrides.Count} EditorID overrides from file");
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"Warning: Error loading subtype overrides YAML: {ex.Message}");
                }
            }
            
            // Load the grunt preservation toggle
            var preserveGruntsGlobal = FindGlobalByEditorId(state, "STFU_PreserveGrunts");
            if (preserveGruntsGlobal == null)
            {
                Console.WriteLine("STFU: [X] Missing STFU_PreserveGrunts - grunt filtering will always be enabled");
            }
            
            // Load the blacklist toggle
            FormKey? blacklistGlobalKey = null;
            var blacklistGlobal = FindGlobalByEditorId(state, "STFU_Blacklist");
            if (blacklistGlobal != null)
            {
                blacklistGlobalKey = blacklistGlobal.FormKey;
            }
            else
            {
                Console.WriteLine("STFU: [X] Missing STFU_Blacklist - blacklisted topics will always be blocked");
            }
            
            // Map each combat subtype to its global variable name
            var combatSubtypeGlobals = new Dictionary<string, string>
            {
                // Combat dialogue
                { "Attack", "STFU_Attack" },
                { "Hit", "STFU_Hit" },
                { "PowerAttack", "STFU_PowerAttack" },
                { "Death", "STFU_Death" },
                { "Block", "STFU_Block" },
                { "Bleedout", "STFU_Bleedout" },
                { "AllyKilled", "STFU_AllyKilled" },
                { "Taunt", "STFU_Taunt" },
                { "NormalToCombat", "STFU_NormalToCombat" },
                { "CombatToNormal", "STFU_CombatToNormal" },
                { "CombatToLost", "STFU_CombatToLost" },
                { "NormalToAlert", "STFU_NormalToAlert" },
                { "AlertToCombat", "STFU_AlertToCombat" },
                { "AlertToNormal", "STFU_AlertToNormal" },
                { "AlertIdle", "STFU_AlertIdle" },
                { "LostIdle", "STFU_LostIdle" },
                { "LostToCombat", "STFU_LostToCombat" },
                { "LostToNormal", "STFU_LostToNormal" },
                { "ObserveCombat", "STFU_ObserveCombat" },
                { "Flee", "STFU_Flee" },
                { "AvoidThreat", "STFU_AvoidThreat" },
                { "Yield", "STFU_Yield" },
                { "AcceptYield", "STFU_AcceptYield" },
                { "Bash", "STFU_Bash" },
                { "PickpocketCombat", "STFU_PickpocketCombat" },
                { "DetectFriendDie", "STFU_DetectFriendDie" },
                
                // Non-combat dialogue
                { "Murder", "STFU_Murder" },
                { "MurderNC", "STFU_MurderNC" },
                { "Assault", "STFU_Assault" },
                { "AssaultNC", "STFU_AssaultNC" },
                { "ActorCollideWithActor", "STFU_ActorCollideWithActor" },
                { "BarterExit", "STFU_BarterExit" },
                { "DestroyObject", "STFU_DestroyObject" },
                { "Goodbye", "STFU_Goodbye" },
                { "Hello", "STFU_Hello" },
                { "KnockOverObject", "STFU_KnockOverObject" },
                { "NoticeCorpse", "STFU_NoticeCorpse" },
                { "PickpocketTopic", "STFU_PickpocketTopic" },
                { "PickpocketNC", "STFU_PickpocketNC" },
                { "LockedObject", "STFU_LockedObject" },
                { "Refuse", "STFU_Refuse" },
                { "MoralRefusal", "STFU_MoralRefusal" },
                { "ExitFavorState", "STFU_ExitFavorState" },
                { "Agree", "STFU_Agree" },
                { "Show", "STFU_Show" },
                { "Trespass", "STFU_Trespass" },
                { "TimeToGo", "STFU_TimeToGo" },
                { "Idle", "STFU_Idle" },
                { "StealFromNC", "STFU_StealFromNC" },
                { "TrespassAgainstNC", "STFU_TrespassAgainstNC" },
                { "PlayerShout", "STFU_PlayerShout" },
                { "PlayerInIronSights", "STFU_PlayerInIronSights" },
                { "PlayerCastProjectileSpell", "STFU_PlayerCastProjectileSpell" },
                { "PlayerCastSelfSpell", "STFU_PlayerCastSelfSpell" },
                { "Steal", "STFU_Steal" },
                { "SwingMeleeWeapon", "STFU_SwingMeleeWeapon" },
                { "ShootBow", "STFU_ShootBow" },
                { "ZKeyObject", "STFU_ZKeyObject" },
                { "StandOnFurniture", "STFU_StandOnFurniture" },
                { "TrainingExit", "STFU_TrainingExit" },
                { "VoicePowerEndLong", "STFU_VoicePowerEndLong" },
                { "VoicePowerEndShort", "STFU_VoicePowerEndShort" },
                { "VoicePowerStartLong", "STFU_VoicePowerStartLong" },
                { "VoicePowerStartShort", "STFU_VoicePowerStartShort" },
                { "WerewolfTransformCrime", "STFU_WerewolfTransformCrime" },
                { "PursueIdleTopic", "STFU_PursueIdleTopic" }
            };
            
            // Combat subtypes for categorization
            var combatSubtypes = new HashSet<string>
            {
                "Attack", "Hit", "PowerAttack", "Death", "Block", "Bleedout", "AllyKilled", "Taunt",
                "NormalToCombat", "CombatToNormal", "CombatToLost", "NormalToAlert", "AlertToCombat",
                "AlertToNormal", "AlertIdle", "LostIdle", "LostToCombat", "LostToNormal", "ObserveCombat",
                "Flee", "AvoidThreat", "Yield", "AcceptYield", "Bash", "PickpocketCombat", "DetectFriendDie",
                "VoicePowerEndLong", "VoicePowerEndShort", "VoicePowerStartLong", "VoicePowerStartShort", "WerewolfTransformCrime"
            };
            
            // Follower dialogue subtypes for categorization
            var followerSubtypes = new HashSet<string>
            {
                "MoralRefusal", "ExitFavorState", "Refuse", "Agree", "Show"
            };
            
            // Follower commentary quest FormKeys (these are Scene subtype but specific quests)
            var followerCommentaryQuests = new HashSet<FormKey>
            {
                FormKey.Factory("04C49D:Skyrim.esm"), // FollowerCommentary01 (Entrance to Dungeons)
                FormKey.Factory("04C6EB:Skyrim.esm"), // FollowerCommentary02 (Follower sees an impressive view)
                FormKey.Factory("04C727:Skyrim.esm")  // FollowerCommentary03 (Danger Ahead)
            };
            
            Console.WriteLine($"STFU: Loaded {followerCommentaryQuests.Count} follower commentary quests for special handling");
            
            // Bard songs quest FormKeys
            var bardSongQuests = new HashSet<FormKey>
            {
                FormKey.Factory("074A55:Skyrim.esm"), // BardSongs
                FormKey.Factory("06E53F:Skyrim.esm")  // BardSongsInstrumental
            };
            
            // Use file-based overrides from YAML only (SNAM is now accurate, no hardcoded overrides needed)
            var manualSubtypeOverrides = fileBasedFormKeyOverrides;
            var manualSubtypeOverridesByEditorId = fileBasedEditorIdOverrides;
            
            // Load all globals upfront
            var loadedGlobals = new Dictionary<string, FormKey>();
            var missingGlobals = new List<string>();
            foreach (var globalName in combatSubtypeGlobals.Values.Distinct())
            {
                var global = FindGlobalByEditorId(state, globalName);
                if (global != null)
                {
                    loadedGlobals[globalName] = global.FormKey;
                }
                else
                {
                    missingGlobals.Add(globalName);
                }
            }
            
            if (loadedGlobals.Count > 0)
            {
                Console.WriteLine($"STFU: [OK] Found {loadedGlobals.Count} subtype globals for MCM control");
            }
            if (missingGlobals.Count > 0)
            {
                Console.WriteLine($"STFU: [X] Missing {missingGlobals.Count} globals - those subtypes won't be blockable: {string.Join(", ", missingGlobals)}");
            }
            
            // Load follower commentary global
            FormKey? followerCommentaryGlobalKey = null;
            var followerCommentaryGlobal = FindGlobalByEditorId(state, "STFU_FollowerCommentary");
            if (followerCommentaryGlobal != null)
            {
                followerCommentaryGlobalKey = followerCommentaryGlobal.FormKey;
            }
            else
            {
                Console.WriteLine($"STFU: [X] Missing STFU_FollowerCommentary - follower commentary quests won't be blockable");
            }
            
            // Load bard songs global
            FormKey? bardSongsGlobalKey = null;
            var bardSongsGlobal = FindGlobalByEditorId(state, "STFU_BardSongs");
            if (bardSongsGlobal != null)
            {
                bardSongsGlobalKey = bardSongsGlobal.FormKey;
            }
            else
            {
                Console.WriteLine($"STFU: [X] Missing STFU_BardSongs - bard songs won't be blockable");
            }
            
            // Load scenes global (for ambient dialogue scenes)
            FormKey? scenesGlobalKey = null;
            var scenesGlobal = FindGlobalByEditorId(state, "STFU_Scenes");
            if (scenesGlobal != null)
            {
                scenesGlobalKey = scenesGlobal.FormKey;
            }
            else
            {
                Console.WriteLine($"STFU: [X] Missing STFU_Scenes - ambient scenes won't be blockable");
            }
            
            // Map SNAM (SubtypeName) abbreviations to full subtype names (discovered from game data)
            var snamToSubtype = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
            {
                { "ACAC", "ActorCollideWithActor" },
                { "ACYI", "AcceptYield" },
                { "AGRE", "Agree" },
                { "ALIL", "AlertIdle" },
                { "ALKL", "AllyKilled" },
                { "ALTC", "AlertToCombat" },
                { "ALTN", "AlertToNormal" },
                { "ASNC", "AssaultNC" },
                { "ASSA", "Assault" },
                { "ATCK", "Attack" },
                { "AVTH", "AvoidThreat" },
                { "BAEX", "BarterExit" },
                { "BASH", "Bash" },
                { "BLED", "Bleedout" },
                { "BLOC", "Block" },
                { "COLO", "CombatToLost" },
                { "COTN", "CombatToNormal" },
                { "CUST", "Custom" },
                { "DETH", "Death" },
                { "DFDA", "DetectFriendDie" },
                { "FEXT", "ExitFavorState" },
                { "FIWE", "ShootBow" },
                { "FLAT", "Flatter" },
                { "FLEE", "Flee" },
                { "GBYE", "Goodbye" },
                { "HELO", "Hello" },
                { "HIT_", "Hit" },
                { "IDAT", "SharedInfo" },
                { "IDLE", "Idle" },
                { "JUMP", "Jump" },
                { "KNOO", "KnockOverObject" },
                { "LOIL", "LostIdle" },
                { "LOOB", "LockedObject" },
                { "LOTC", "LostToCombat" },
                { "LOTN", "LostToNormal" },
                { "LWBS", "LeaveWaterBreath" },
                { "MREF", "MoralRefusal" },
                { "MUNC", "MurderNC" },
                { "MURD", "Murder" },
                { "NOTA", "NormalToAlert" },
                { "NOTC", "NormalToCombat" },
                { "NOTI", "NoticeCorpse" },
                { "OBCO", "ObserveCombat" },
                { "PCPS", "PlayerCastProjectileSpell" },
                { "PCSH", "PlayerShout" },
                { "PFGT", "ForceGreet" },
                { "PICC", "PickpocketCombat" },
                { "PICN", "PickpocketNC" },
                { "PICT", "PickpocketTopic" },
                { "PIRN", "PlayerInIronSights" },
                { "POAT", "PowerAttack" },
                { "REFU", "Refuse" },
                { "RUMO", "Rumors" },
                { "SCEN", "Scene" },
                { "SERU", "ServiceRefusal" },
                { "SHOW", "Show" },
                { "STEA", "Steal" },
                { "STFN", "StealFromNC" },
                { "STOF", "StandOnFurniture" },
                { "SWMW", "SwingMeleeWeapon" },
                { "TAUT", "Taunt" },
                { "TITG", "TimeToGo" },
                { "TRAN", "TrespassAgainstNC" },
                { "TRES", "Trespass" },
                { "PURS", "PursueIdleTopic" },
                { "VPEL", "VoicePowerEndLong" },
                { "VPES", "VoicePowerEndShort" },
                { "VPSL", "VoicePowerStartLong" },
                { "VPSS", "VoicePowerStartShort" },
                { "WTCR", "WerewolfTransformCrime" },
                { "YIEL", "Yield" },
                { "ZKEY", "ZKeyObject" }
            };

            
            Console.WriteLine("STFU: Starting dialogue patching...");
            
            int combatTopicCount = 0;
            int combatResponseCount = 0;
            int bardSongTopicCount = 0;
            int bardSongResponseCount = 0;

            int nonCombatTopicCount = 0;
            int nonCombatResponseCount = 0;
            int processedTopics = 0;
            bool encounteredCorruption = false;
            var encounteredSubtypes = new HashSet<string>();
            int skippedByConfigCount = 0; // Track topics skipped by config filters
            int skippedByVanillaOnlyCount = 0; // Track topics skipped by VanillaOnly setting
            
            // Track topics patched per subtype for diagnostic output
            var subtypeTopicCounts = new Dictionary<string, int>();
            
            // Track which (responseFormKey, globalKey) combinations we've already decided on
            // Don't cache the actual response objects, just the decision
            var responseGlobalDecisions = new Dictionary<(FormKey, FormKey?), bool>();
            
            // Track which response FormKeys we've already written to the patch
            // When we encounter the same FormKey again, we need to create a NEW response with a new FormKey
            var writtenResponseKeys = new HashSet<FormKey>();
            
            // DialogResponses don't have Subtype - we need to iterate DialogTopics instead!
            // DialogTopics contain the Subtype info (Idle, Combat, etc.) and link to responses
            // Wrap the entire enumeration to catch corruption errors during iteration
            
            Console.WriteLine("STFU: Collecting ALL dialogue topic overrides from EVERY mod in load order...");
            
            // First pass: Iterate through EVERY mod and collect ALL versions of each topic by FormKey
            // This is the ONLY way to get responses added by multiple different mods
            var topicsByFormKey = new Dictionary<FormKey, List<IDialogTopicGetter>>();
            int totalTopicsFound = 0;
            
            try
            {
                foreach (var modListing in state.LoadOrder.PriorityOrder)
                {
                    try
                    {
                        // Collect from main DialogTopics collection (includes quest dialogue)
                        int countInMod = 0;
                        foreach (var topic in modListing.Mod!.DialogTopics)
                        {
                            countInMod++;
                            totalTopicsFound++;
                            var formKey = topic.FormKey;
                            
                            if (!topicsByFormKey.ContainsKey(formKey))
                            {
                                topicsByFormKey[formKey] = new List<IDialogTopicGetter>();
                            }
                            topicsByFormKey[formKey].Add(topic);
                        }
                        
                        if (countInMod > 0)
                        {
                            Console.WriteLine($"  - {modListing.ModKey}: {countInMod} topics");
                        }
                    }
                    catch (Exception ex)
                    {
                        Console.WriteLine($"Warning: Skipping corrupted mod {modListing.ModKey}: {ex.Message}");
                        continue;
                    }
                }
            }
            catch (Exception ex)
            {
                encounteredCorruption = true;
                Console.WriteLine($"Warning: Encountered error during enumeration: {ex.Message}");
            }
            
            Console.WriteLine($"STFU: Collected {totalTopicsFound} total topic references from all mods");
            Console.WriteLine($"STFU: Found {topicsByFormKey.Count} unique dialogue topics across all mods, now processing...");
            
            // Build quest FormKey → EditorID dictionary for fast lookups (avoids O(n*m) nested iteration)
            Console.WriteLine("STFU: Building quest lookup dictionary...");
            var questFormKeyToEditorId = new Dictionary<FormKey, string>();
            foreach (var quest in state.LoadOrder.PriorityOrder.Quest().WinningOverrides())
            {
                if (!string.IsNullOrEmpty(quest.EditorID))
                {
                    questFormKeyToEditorId[quest.FormKey] = quest.EditorID;
                }
            }
            Console.WriteLine($"STFU: Loaded {questFormKeyToEditorId.Count} quest EditorIDs for lookup");
            
            // Second pass: process each unique topic with ALL its responses from ALL mod overrides
            var patchedTopicKeys = new HashSet<FormKey>(); // Track topics we've already patched
            var gruntDecisionCache = new Dictionary<FormKey, (bool isGrunt, bool shouldBlock)>(); // Cache grunt/block decisions
            
            try
            {
                foreach (var kvp in topicsByFormKey)
                {
                    try
                    {
                        processedTopics++;
                        
                        // Get the winning override (last one in load order) to check type and use for our override
                        var winningTopic = kvp.Value.Last();
                        
                        // Use SNAM (SubtypeName) exclusively - it's more accurate than DATA Subtype
                        var rawSubtype = winningTopic.SubtypeName.Type;
                        
                        // Convert SNAM abbreviation to full subtype name
                        var subtype = snamToSubtype.TryGetValue(rawSubtype, out var fullName) ? fullName : rawSubtype;
                        encounteredSubtypes.Add(subtype);
                        
                        // Check if this subtype is disabled in config
                        // If no filter settings defined, all subtypes are enabled by default
                        if (hasFilterConfig && disabledSubtypes.Contains(subtype))
                        {
                            // Skip this topic - subtype is disabled in config
                            skippedByConfigCount++;
                            continue;
                        }
                        
                        // Early blacklist check - blacklist overrides vanillaOnly
                        bool isEarlyBlacklisted = blacklistedTopics.Contains(winningTopic.FormKey) ||
                            (!string.IsNullOrEmpty(winningTopic.EditorID) && blacklistedTopicEditorIds.Contains(winningTopic.EditorID)) ||
                            blacklistedPlugins.Contains(winningTopic.FormKey.ModKey.FileName.String);
                        
                        // Check VanillaOnly setting - skip if topic is not from vanilla plugins
                        // BUT allow blacklisted topics through (they'll be blocked later)
                        if (!isEarlyBlacklisted && vanillaOnly && !vanillaPlugins.Contains(winningTopic.FormKey.ModKey.FileName))
                        {
                            // Skip this topic - not from vanilla and VanillaOnly is enabled
                            skippedByVanillaOnlyCount++;
                            continue;
                        }
                        
                        // Check for file-based subtype override (from YAML only)
                        string? overrideSubtype = null;
                        if (manualSubtypeOverrides.TryGetValue(winningTopic.FormKey, out var formKeyOverride))
                        {
                            overrideSubtype = formKeyOverride;
                            subtype = overrideSubtype;
                        }
                        else if (!string.IsNullOrEmpty(winningTopic.EditorID) && 
                                 manualSubtypeOverridesByEditorId.TryGetValue(winningTopic.EditorID, out var editorIdOverride))
                        {
                            overrideSubtype = editorIdOverride;
                            subtype = overrideSubtype;
                        }
                        
                        // Check if ANY version of this topic has a configured subtype
                        // Prioritize known fix mods (USSEP) over vanilla for subtype selection
                        FormKey? globalKey = null;
                        string? matchedSubtype = null;
                        
                        // If we have a manual override, check if that subtype has a global configured
                        if (overrideSubtype != null && combatSubtypeGlobals.ContainsKey(overrideSubtype))
                        {
                            if (loadedGlobals.TryGetValue(combatSubtypeGlobals[overrideSubtype], out var overrideKey))
                            {
                                globalKey = overrideKey;
                                matchedSubtype = overrideSubtype;
                            }
                        }
                        
                        // Trusted mods that fix vanilla categorization
                        var trustedFixMods = new HashSet<string>(StringComparer.OrdinalIgnoreCase)
                        {
                            "Unofficial Skyrim Special Edition Patch.esp",
                            "USSEP.esp"
                        };
                        
                        // If no manual override or it didn't have a global, check ALL versions for configured subtypes
                        if (!globalKey.HasValue)
                        {
                            // Check ALL versions for configured subtypes, prioritizing trusted fix mods
                            foreach (var topicVersion in kvp.Value)
                            {
                                // Use SNAM (SubtypeName) exclusively - it's more accurate than DATA Subtype
                                var versionRawSubtype = topicVersion.SubtypeName.Type;
                                
                                // Convert SNAM abbreviation to full subtype name
                                var versionSubtype = snamToSubtype.TryGetValue(versionRawSubtype, out var versionFullName) 
                                    ? versionFullName 
                                    : versionRawSubtype;
                                
                                if (combatSubtypeGlobals.ContainsKey(versionSubtype) && 
                                    loadedGlobals.TryGetValue(combatSubtypeGlobals[versionSubtype], out var versionKey))
                                {
                                    // Prioritize trusted fix mods
                                    if (trustedFixMods.Contains(topicVersion.FormKey.ModKey.FileName.String))
                                    {
                                        globalKey = versionKey;
                                        matchedSubtype = versionSubtype;
                                        break; // Use trusted fix mod immediately
                                    }
                                    // Save vanilla as fallback
                                    else if (topicVersion.FormKey.ModKey.FileName.String.Equals("Skyrim.esm", StringComparison.OrdinalIgnoreCase))
                                    {
                                        if (!globalKey.HasValue)
                                        {
                                            globalKey = versionKey;
                                            matchedSubtype = versionSubtype;
                                        }
                                    }
                                    // Other mods - only use if nothing else found
                                    else if (!globalKey.HasValue)
                                    {
                                        globalKey = versionKey;
                                        matchedSubtype = versionSubtype;
                                    }
                                }
                            }
                        }
                        
                        // Check if topic is whitelisted (skip if whitelisted)
                        bool isWhitelisted = whitelistedTopics.Contains(winningTopic.FormKey) ||
                            (!string.IsNullOrEmpty(winningTopic.EditorID) && whitelistedTopicEditorIds.Contains(winningTopic.EditorID)) ||
                            whitelistedPlugins.Contains(winningTopic.FormKey.ModKey.FileName.String);
                        if (isWhitelisted)
                            continue;
                        
                        // Check if topic's quest is whitelisted (skip if whitelisted)
                        bool isQuestWhitelisted = false;
                        if (winningTopic.Quest != null)
                        {
                            isQuestWhitelisted = whitelistedQuestFormKeys.Contains(winningTopic.Quest.FormKey);
                            
                            // Also check by EditorID using pre-built dictionary (O(1) lookup)
                            if (!isQuestWhitelisted && questFormKeyToEditorId.TryGetValue(winningTopic.Quest.FormKey, out var questEditorId))
                            {
                                isQuestWhitelisted = whitelistedQuestEditorIds.Contains(questEditorId);
                            }
                        }
                        if (isQuestWhitelisted)
                            continue;
                        
                        // Use the early blacklist check we already did
                        bool isBlacklisted = isEarlyBlacklisted;
                        
                        // Check if this is a follower commentary quest (Scene subtype but specific quests)
                        bool isFollowerCommentaryQuest = false;
                        if (winningTopic.Quest != null && followerCommentaryQuests.Contains(winningTopic.Quest.FormKey))
                        {
                            isFollowerCommentaryQuest = true;
                            if (followerCommentaryGlobalKey.HasValue)
                            {
                                globalKey = followerCommentaryGlobalKey.Value;
                                matchedSubtype = "FollowerCommentary";
                            }
                        }
                        
                        // Check if this is a bard song quest
                        bool isBardSongQuest = false;
                        if (winningTopic.Quest != null && bardSongQuests.Contains(winningTopic.Quest.FormKey))
                        {
                            isBardSongQuest = true;
                            if (bardSongsGlobalKey.HasValue)
                            {
                                globalKey = bardSongsGlobalKey.Value;
                                matchedSubtype = "BardSongs";
                            }
                        }
                        
                        // Skip SharedInfo topics entirely - they're reference pools used by other topics
                        // We'll patch the responses when they're referenced by non-SharedInfo topics
                        if (matchedSubtype == "SharedInfo" || subtype == "SharedInfo")
                            continue;
                        
                        // Skip Custom topics (follower commands) UNLESS explicitly blacklisted or overridden
                        // This prevents accidental quest breakage while allowing user-defined filtering
                        bool isCustom = matchedSubtype == "Custom" || subtype == "Custom";
                        if (isCustom && !isBlacklisted && overrideSubtype == null)
                            continue;
                        
                        // Skip if not blacklisted AND no global configured for this subtype
                        if (!isBlacklisted && !globalKey.HasValue)
                            continue;
                        
                        // Skip if we've already processed this topic
                        if (patchedTopicKeys.Contains(winningTopic.FormKey))
                            continue;
                        
                        patchedTopicKeys.Add(winningTopic.FormKey);
                        
                        // Collect responses from ALL versions of this topic (mods can add new responses to vanilla topics)
                        var allResponses = new List<IDialogResponsesGetter>();
                        var seenResponseKeys = new HashSet<FormKey>();
                        
                        foreach (var topicVersion in kvp.Value)
                        {
                            if (topicVersion.Responses != null)
                            {
                                foreach (var response in topicVersion.Responses)
                                {
                                    if (!seenResponseKeys.Contains(response.FormKey))
                                    {
                                        allResponses.Add(response);
                                        seenResponseKeys.Add(response.FormKey);
                                    }
                                }
                            }
                        }
                        
                        // Skip if no responses
                        if (allResponses.Count == 0)
                            continue;
                        
                        // Check for scripts if SafeMode is enabled
                        if (safeMode)
                        {
                            bool hasScripts = false;
                            foreach (var responseGroup in allResponses)
                            {
                                if (responseGroup.VirtualMachineAdapter != null && responseGroup.VirtualMachineAdapter.Scripts != null && responseGroup.VirtualMachineAdapter.Scripts.Count > 0)
                                {
                                    hasScripts = true;
                                    break;
                                }
                            }
                            
                            // Skip patching if scripts detected
                            if (hasScripts)
                            {
                                Console.WriteLine($"STFU: SafeMode - Skipping topic with scripts: '{winningTopic.EditorID}' [{winningTopic.FormKey}] (subtype: {matchedSubtype ?? subtype})");
                                continue;
                            }
                        }
                        
                        // Create override in our patch (using the winning override) in our patch (using the winning override)
                        var mutableTopic = state.PatchMod.DialogTopics.GetOrAddAsOverride(winningTopic);
                        
                        // Add blocking condition for this subtype (or use blacklist global for blacklisted topics)
                        int responsesAdded = AddBlockingConditionToResponseList(state, allResponses, mutableTopic, globalKey, preserveGruntsGlobal?.FormKey, isBlacklisted, blacklistGlobalKey, gruntDecisionCache, responseGlobalDecisions, writtenResponseKeys);
                        
                        // Track per-subtype counts
                        var countedSubtype = matchedSubtype ?? subtype;
                        if (!subtypeTopicCounts.ContainsKey(countedSubtype))
                            subtypeTopicCounts[countedSubtype] = 0;
                        subtypeTopicCounts[countedSubtype]++;
                        
                        if (combatSubtypes.Contains(matchedSubtype ?? subtype))
                        {
                            combatTopicCount++;
                            combatResponseCount += responsesAdded;
                        }
                        else if (isBardSongQuest)
                        {
                            bardSongTopicCount++;
                            bardSongResponseCount += responsesAdded;
                        }
                        else
                        {
                            nonCombatTopicCount++;
                            nonCombatResponseCount += responsesAdded;
                        }
                    }
                    catch (Exception ex)
                    {
                        // Skip individual problematic records
                        Console.WriteLine($"Warning: Skipping topic: {ex.Message}");
                        continue;
                    }
                }
            }
            catch (Exception ex)
            {
                // Catch enumeration errors (e.g., corrupted ESP files)
                encounteredCorruption = true;
                Console.WriteLine($"Warning: Encountered corrupted plugin during enumeration: {ex.Message}");
                Console.WriteLine($"Warning: Processed {processedTopics} topics before error. Continuing with patch generation...");
            }
            
            Console.WriteLine($"STFU: Patching complete!");
            Console.WriteLine($"  - Processed {processedTopics} total dialogue topics");
            Console.WriteLine($"  - Combat dialogue: {combatTopicCount} topics ({combatResponseCount} responses)");
            Console.WriteLine($"  - Bard songs: {bardSongTopicCount} topics ({bardSongResponseCount} responses)");
            Console.WriteLine($"  - Non-combat dialogue: {nonCombatTopicCount} topics ({nonCombatResponseCount} responses)");
            Console.WriteLine($"  - Total blocked: {combatTopicCount + bardSongTopicCount + nonCombatTopicCount} topics ({combatResponseCount + bardSongResponseCount + nonCombatResponseCount} responses)");
            
            // Combine all scene patching into a single pass for performance
            Console.WriteLine();
            Console.WriteLine("STFU: Patching quest scenes...");
            
            // Build quest EditorID → FormKey dictionary for fast lookups (avoids O(n*m) nested iteration)
            var questEditorIdToFormKey = new Dictionary<string, FormKey>(StringComparer.OrdinalIgnoreCase);
            foreach (var quest in state.LoadOrder.PriorityOrder.Quest().WinningOverrides())
            {
                if (!string.IsNullOrEmpty(quest.EditorID))
                {
                    questEditorIdToFormKey[quest.EditorID] = quest.FormKey;
                }
            }
            
            // Build bard quest FormKeys
            var bardQuestFormKeys = new HashSet<FormKey>();
            if (bardSongsGlobalKey.HasValue)
            {
                var bardQuestEditorIds = new[] { "BardSongs", "BardSongsInstrumental", "BardAudienceQuest" };
                foreach (var questEditorId in bardQuestEditorIds)
                {
                    if (questEditorIdToFormKey.TryGetValue(questEditorId, out var formKey))
                    {
                        bardQuestFormKeys.Add(formKey);
                    }
                }
            }
            
            // Build blacklisted quest FormKeys (combine EditorID lookups with direct FormKeys)
            var blacklistedQuestFormKeysForScenes = new HashSet<FormKey>();
            if ((blacklistedQuestEditorIds.Count > 0 || blacklistedQuestFormKeys.Count > 0) && blacklistGlobalKey.HasValue)
            {
                // Add FormKeys from EditorID lookups
                foreach (var questEditorId in blacklistedQuestEditorIds)
                {
                    if (whitelistedQuestEditorIds.Contains(questEditorId))
                    {
                        continue;
                    }
                    if (questEditorIdToFormKey.TryGetValue(questEditorId, out var formKey))
                    {
                        blacklistedQuestFormKeysForScenes.Add(formKey);
                    }
                }
                
                // Add direct FormKeys (check whitelist)
                foreach (var questFormKey in blacklistedQuestFormKeys)
                {
                    if (!whitelistedQuestFormKeys.Contains(questFormKey))
                    {
                        blacklistedQuestFormKeysForScenes.Add(questFormKey);
                    }
                }
            }
            
            // Build ambient scene FormKeys from hardcoded EditorID list
            var scenesFormKeys = new HashSet<FormKey>();
            if (hardcodedSceneEditorIds.Count > 0 && scenesGlobalKey.HasValue)
            {
                foreach (var scene in state.LoadOrder.PriorityOrder.Scene().WinningOverrides())
                {
                    if (!string.IsNullOrEmpty(scene.EditorID) && hardcodedSceneEditorIds.Contains(scene.EditorID))
                    {
                        scenesFormKeys.Add(scene.FormKey);
                    }
                }
                Console.WriteLine($"STFU: Found {scenesFormKeys.Count} ambient scene FormKeys from EditorID list");
            }
            
            // Single pass through all scenes for bard songs, blacklisted quests, and ambient scenes
            int bardScenesPatched = 0;
            int blacklistScenesPatched = 0;
            int ambientScenesPatched = 0;
            
            if (bardQuestFormKeys.Count > 0 || blacklistedQuestFormKeysForScenes.Count > 0 || scenesFormKeys.Count > 0)
            {
                foreach (var scene in state.LoadOrder.PriorityOrder.Scene().WinningOverrides())
                {
                    var isBardScene = bardQuestFormKeys.Contains(scene.Quest.FormKey);
                    var isBlacklistedScene = blacklistedQuestFormKeysForScenes.Contains(scene.Quest.FormKey);
                    var isAmbientScene = scenesFormKeys.Contains(scene.FormKey);
                    
                    if (isBardScene || isBlacklistedScene || isAmbientScene)
                    {
                        var mutableScene = state.PatchMod.Scenes.GetOrAddAsOverride(scene);
                        
                        // Determine which global to use (priority: bard > blacklist > ambient)
                        var globalKey = isBardScene ? bardSongsGlobalKey!.Value : 
                                       isBlacklistedScene ? blacklistGlobalKey!.Value :
                                       scenesGlobalKey!.Value;
                        
                        // Patch all phases to fully block scenes
                        if (mutableScene.Phases != null)
                        {
                            foreach (var phase in mutableScene.Phases)
                            {
                                var phaseCondition = new ConditionFloat
                                {
                                    Data = new FunctionConditionData
                                    {
                                        Function = Condition.Function.GetGlobalValue,
                                        ParameterOneRecord = new FormLink<IGlobalGetter>(globalKey)
                                    },
                                    ComparisonValue = 0f,
                                    CompareOperator = CompareOperator.EqualTo
                                };
                                phase.StartConditions.Insert(0, phaseCondition);
                            }
                        }
                        
                        if (isBardScene) bardScenesPatched++;
                        if (isBlacklistedScene) blacklistScenesPatched++;
                        if (isAmbientScene) ambientScenesPatched++;
                    }
                }
            }
            
            if (bardQuestFormKeys.Count > 0)
            {
                Console.WriteLine($"  - Bard songs: {bardScenesPatched} scenes");
            }
            if (blacklistedQuestFormKeysForScenes.Count > 0)
            {
                Console.WriteLine($"  - Blacklisted quests: {blacklistScenesPatched} scenes");
            }
            if (scenesFormKeys.Count > 0)
            {
                Console.WriteLine($"  - Ambient scenes: {ambientScenesPatched} scenes");
            }
            
            // Patch blacklisted scenes by adding blocking conditions
            if (blacklistedSceneEditorIds.Count > 0 && blacklistGlobalKey.HasValue)
            {
                Console.WriteLine();
                Console.WriteLine($"STFU: Patching {blacklistedSceneEditorIds.Count} blacklisted scenes...");
                
                foreach (var sceneEditorId in blacklistedSceneEditorIds)
                {
                    // Skip if whitelisted
                    if (whitelistedSceneEditorIds.Contains(sceneEditorId))
                    {
                        Console.WriteLine($"  - Skipped whitelisted scene: {sceneEditorId}");
                        continue;
                    }
                    
                    try
                    {
                        var scene = state.LoadOrder.PriorityOrder.Scene().WinningOverrides()
                            .FirstOrDefault(s => s.EditorID?.Equals(sceneEditorId, StringComparison.OrdinalIgnoreCase) == true);
                        
                        if (scene != null)
                        {
                            var mutableScene = state.PatchMod.Scenes.GetOrAddAsOverride(scene);
                            
                            // Patch all phases to fully block scenes
                            if (mutableScene.Phases != null)
                            {
                                foreach (var phase in mutableScene.Phases)
                                {
                                    var phaseCondition = new ConditionFloat
                                    {
                                        Data = new FunctionConditionData
                                        {
                                            Function = Condition.Function.GetGlobalValue,
                                            ParameterOneRecord = new FormLink<IGlobalGetter>(blacklistGlobalKey.Value)
                                        },
                                        ComparisonValue = 0f,
                                        CompareOperator = CompareOperator.EqualTo
                                    };
                                    phase.StartConditions.Insert(0, phaseCondition);
                                }
                            }
                            
                            Console.WriteLine($"  - Patched scene: {sceneEditorId}");
                        }
                        else
                        {
                            Console.WriteLine($"Warning: Scene not found for patching: {sceneEditorId}");
                        }
                    }
                    catch (Exception ex)
                    {
                        Console.WriteLine($"Warning: Failed to patch scene {sceneEditorId}: {ex.Message}");
                    }
                }
            }
            else if (blacklistedSceneEditorIds.Count > 0 && !blacklistGlobalKey.HasValue)
            {
                Console.WriteLine($"Warning: {blacklistedSceneEditorIds.Count} scenes in blacklist but STFU_Blacklist global not found - scenes not patched");
            }
            
            if (encounteredCorruption)
            {
                Console.WriteLine();
                Console.WriteLine($"⚠ Note: Some plugins had corruption errors during processing");
            }
            
            // Report config filtering stats
            if (skippedByConfigCount > 0 || skippedByVanillaOnlyCount > 0)
            {
                Console.WriteLine();
                Console.WriteLine($"STFU: Config filtering summary:");
                if (skippedByConfigCount > 0)
                {
                    Console.WriteLine($"  - Skipped {skippedByConfigCount} topics (disabled subtypes in config)");
                }
                if (skippedByVanillaOnlyCount > 0)
                {
                    Console.WriteLine($"  - Skipped {skippedByVanillaOnlyCount} topics (VanillaOnly mode)");
                }
            }
        }

        private static int BlankFollowerResponses(IPatcherState<ISkyrimMod, ISkyrimModGetter> state, List<IDialogResponsesGetter> allResponses, IDialogTopic mutableTopic, HashSet<FormKey> writtenResponseKeys)
        {
            try
            {
                if (mutableTopic.Responses != null)
                {
                    // Deduplicate responses by FormKey
                    var uniqueResponses = new Dictionary<FormKey, IDialogResponsesGetter>();
                    foreach (var response in allResponses)
                    {
                        uniqueResponses[response.FormKey] = response;
                    }
                    
                    var responsesToAdd = new List<DialogResponses>();
                    
                    foreach (var response in uniqueResponses.Values)
                    {
                        // Create fresh DeepCopy
                        var workingCopy = response.DeepCopy();
                        
                        DialogResponses mutableResponse;
                        
                        // Check if this FormKey has already been written to the patch
                        if (writtenResponseKeys.Contains(response.FormKey))
                        {
                            // Create NEW response with NEW FormKey
                            mutableResponse = workingCopy.Duplicate(state.PatchMod.GetNextFormKey());
                            
                            // Copy conditions
                            if (workingCopy.Conditions != null)
                            {
                                foreach (var cond in workingCopy.Conditions)
                                {
                                    mutableResponse.Conditions.Add(cond.DeepCopy());
                                }
                            }
                            
                            // Copy responses with blanked text
                            if (workingCopy.Responses != null)
                            {
                                foreach (var resp in workingCopy.Responses)
                                {
                                    var blankResp = resp.DeepCopy();
                                    blankResp.Text = " "; // Blank the NAM1 - Response Text
                                    mutableResponse.Responses.Add(blankResp);
                                }
                            }
                        }
                        else
                        {
                            // First time seeing this FormKey
                            mutableResponse = workingCopy;
                            writtenResponseKeys.Add(response.FormKey);
                            
                            // Blank the response text
                            if (mutableResponse.Responses != null)
                            {
                                foreach (var resp in mutableResponse.Responses)
                                {
                                    resp.Text = " "; // Blank the NAM1 - Response Text
                                }
                            }
                        }
                        
                        responsesToAdd.Add(mutableResponse);
                    }
                    
                    // Replace all responses in the topic
                    mutableTopic.Responses.Clear();
                    foreach (var resp in responsesToAdd)
                    {
                        mutableTopic.Responses.Add(resp);
                    }
                    
                    return responsesToAdd.Count;
                }
                return 0;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"ERROR blanking follower responses: {ex.Message}");
                return 0;
            }
        }



        private static bool IsGruntSound(string text)
        {
            if (string.IsNullOrWhiteSpace(text)) return false;
            
            text = text.Trim();
            
            // Only use explicit list of known vanilla grunts (no pattern matching)
            var knownGrunts = new HashSet<string>(StringComparer.OrdinalIgnoreCase)
            {
                // From DialogueGeneric
                "Agh!", "Oof!", "Nargh!", "Argh!", "Weergh!", "Yeagh!", "Yearrgh!", "Hyargh!", 
                "Rrarggh!", "Grrargh!", "Aggghh!", "Nyyarrggh!", "Hsssss!", "Agh...", "Ugh...", 
                "Hunh...", "Gah...", "Nuh...", "Gah!", "Unf!", "Grrh!", "Nnh!", 
                "Hhyyaarargghhhh!", "Rrrraaaaarrggghhhh!", "Yyyaaaarrgghh!", "Aaaayyyaarrrrgghh!", 
                "Nnyyyaarrgghh!", "Hunh!", "Gah!", "Yah!", "Rargh!",
                
                // From unique NPC voices
                "Aghh..", "Nuhn!", "Aggh!", "Arrggh!", "Wagh!", "Unh!", "Yeeeaarrggh!", 
                "Hhyyaarrgghh!", "Nnnnyyaarrgghh!", "Agggh!", "Grrarrgh!", "Naggh!", "Yeeaaaggh!", "Yiiee!", "Nyargh!",
                
                // Additional grunts
                "Grrragh!", "Gaaaah!", "Aaaaah!", "Nuh!", "Yeeeaaahhh!", "Ahhhhh.....", 
                "Hhhhuuuuuhhh....", "Aiiiieee!", "Ahhhhhhhh!", "Eyyaargh!", "Grraaagghh!", 
                "Raarrgh!", "Huuragh!", "Grrrarrggh!", "Raaarr!", "Raaarrgh!", "Grragh!", 
                "Arugh!", "Yaaaargh!", "Ffffeeeargh!", "Rlyehhhgh1", "Heyargh!", "Grrr!", 
                "Uf!", "Egh!", "Yergh!"
            };
            
            return knownGrunts.Contains(text);
        }

        private static int AddBlockingConditionToResponseList(IPatcherState<ISkyrimMod, ISkyrimModGetter> state, List<IDialogResponsesGetter> allResponses, IDialogTopic mutableTopic, FormKey? globalKey, FormKey? preserveGruntsKey, bool isBlacklisted, FormKey? blacklistGlobalKey, Dictionary<FormKey, (bool isGrunt, bool shouldBlock)> decisionCache, Dictionary<(FormKey, FormKey?), bool> responseGlobalDecisions, HashSet<FormKey> writtenResponseKeys)
        {
            try
            {
                if (mutableTopic.Responses != null)
                {
                    // Deduplicate responses by FormKey within this topic
                    var uniqueResponses = new Dictionary<FormKey, IDialogResponsesGetter>();
                    foreach (var response in allResponses)
                    {
                        uniqueResponses[response.FormKey] = response;
                    }
                    
                    int gruntCount = 0;
                    int blockedCount = 0;
                    
                    // Build response list for THIS topic
                    var responsesToAdd = new List<DialogResponses>();
                    
                    foreach (var response in uniqueResponses.Values)
                    {
                        // Determine grunt/block status (use cached decision if available)
                        bool isGrunt;
                        bool shouldBlock;
                        
                        if (decisionCache.TryGetValue(response.FormKey, out var decision))
                        {
                            isGrunt = decision.isGrunt;
                            shouldBlock = decision.shouldBlock;
                        }
                        else
                        {
                            // First time seeing this response - determine if it's a grunt
                            isGrunt = false;
                            try
                            {
                                if (response.Responses != null)
                                {
                                    foreach (var resp in response.Responses)
                                    {
                                        string text = resp.Text?.String ?? "";
                                        if (IsGruntSound(text))
                                        {
                                            isGrunt = true;
                                            break;
                                        }
                                    }
                                }
                            }
                            catch
                            {
                                // If we can't get text, assume it's not a grunt
                            }
                            
                            shouldBlock = true;
                            decisionCache[response.FormKey] = (isGrunt, shouldBlock);
                        }
                        
                        // Create fresh DeepCopy for THIS topic (each topic needs its own copy)
                        var workingCopy = response.DeepCopy();
                        
                        DialogResponses mutableResponse;
                        
                        // Check if this FormKey has already been written to the patch
                        if (writtenResponseKeys.Contains(response.FormKey))
                        {
                            // This response is shared across topics - need to create a NEW response with a NEW FormKey
                            mutableResponse = workingCopy.Duplicate(state.PatchMod.GetNextFormKey());
                            
                            // Manually copy all data from workingCopy to the new response
                            if (workingCopy.Responses != null)
                            {
                                foreach (var resp in workingCopy.Responses)
                                {
                                    mutableResponse.Responses.Add(resp.DeepCopy());
                                }
                            }
                            if (workingCopy.Conditions != null)
                            {
                                foreach (var cond in workingCopy.Conditions)
                                {
                                    mutableResponse.Conditions.Add(cond.DeepCopy());
                                }
                            }
                        }
                        else
                        {
                            // First time seeing this FormKey - use the working copy as-is
                            mutableResponse = workingCopy;
                            writtenResponseKeys.Add(response.FormKey);
                        }
                        
                        // Now apply our blocking conditions (these will be preserved)
                        if (isGrunt && !isBlacklisted)
                        {
                            if (preserveGruntsKey.HasValue)
                            {
                                var gruntCondition = new ConditionFloat
                                {
                                    Data = new FunctionConditionData
                                    {
                                        Function = Condition.Function.GetGlobalValue,
                                        ParameterOneRecord = new FormLink<IGlobalGetter>(preserveGruntsKey.Value)
                                    },
                                    ComparisonValue = 0f,
                                    CompareOperator = CompareOperator.EqualTo
                                };
                                mutableResponse.Conditions.Insert(0, gruntCondition);
                            }
                        }
                        else if (shouldBlock)
                        {
                            if (isBlacklisted)
                            {
                                // Blacklisted topics use the blacklist global if available
                                if (blacklistGlobalKey.HasValue)
                                {
                                    var blacklistCondition = new ConditionFloat
                                    {
                                        Data = new FunctionConditionData
                                        {
                                            Function = Condition.Function.GetGlobalValue,
                                            ParameterOneRecord = new FormLink<IGlobalGetter>(blacklistGlobalKey.Value)
                                        },
                                        ComparisonValue = 0f,
                                        CompareOperator = CompareOperator.EqualTo
                                    };
                                    mutableResponse.Conditions.Insert(0, blacklistCondition);
                                }
                                else
                                {
                                    // No blacklist global - use impossible condition to always block
                                    var alwaysFailCondition = new ConditionFloat
                                    {
                                        Data = new FunctionConditionData
                                        {
                                            Function = Condition.Function.GetGlobalValue,
                                            ParameterOneRecord = new FormLink<IGlobalGetter>(FormKey.Factory("000000:Skyrim.esm"))
                                        },
                                        ComparisonValue = 0f,
                                        CompareOperator = CompareOperator.EqualTo
                                    };
                                    alwaysFailCondition.ComparisonValue = 1f;
                                    mutableResponse.Conditions.Insert(0, alwaysFailCondition);
                                }
                            }
                            else if (globalKey.HasValue)
                            {
                                var condition = new ConditionFloat
                                {
                                    Data = new FunctionConditionData
                                    {
                                        Function = Condition.Function.GetGlobalValue,
                                        ParameterOneRecord = new FormLink<IGlobalGetter>(globalKey.Value)
                                    },
                                    ComparisonValue = 0f,
                                    CompareOperator = CompareOperator.EqualTo
                                };
                                mutableResponse.Conditions.Insert(0, condition);
                            }
                        }
                        
                        // Count for statistics
                        if (isGrunt && !isBlacklisted)
                        {
                            gruntCount++;
                        }
                        else if (shouldBlock)
                        {
                            blockedCount++;
                        }
                        
                        // Add to THIS topic's response list
                        responsesToAdd.Add(mutableResponse);
                    }
                    
                    // Now clear and set all responses at once
                    mutableTopic.Responses.Clear();
                    mutableTopic.Responses.AddRange(responsesToAdd);
                    
                    // Return the number of unique responses we actually blocked (not counting preserved grunts)
                    return blockedCount;
                }
                return 0;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error: Failed to block topic: {ex.Message}");
                return 0;
            }
        }

        private static IGlobalGetter? FindGlobalByEditorId(IPatcherState<ISkyrimMod, ISkyrimModGetter> state, string editorId)
        {
            try
            {
                var result = state.LoadOrder.PriorityOrder.Global().WinningOverrides()
                    .FirstOrDefault(g => g.EditorID?.Equals(editorId, StringComparison.OrdinalIgnoreCase) == true);
                return result;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Warning: Warning during global lookup: {ex.Message}");
                return null;
            }
        }
    }
}





