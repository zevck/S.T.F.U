/*
 * STFU - a Skyrim SKSE plugin for silencing and filtering NPC dialogue.
 * Copyright (C) 2026 Zevick
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "Config.h"
#include "ConfigInternal.h"
#include "EditorID.h"
#include "DialogueDatabase.h"
#include <spdlog/spdlog.h>
#include <unordered_set>
#include <vector>

namespace Config
{
    
    void InitializeHardcodedScenes()
    {
        // Hardcoded ambient scene topic/scene EditorIDs from Synthesis patcher
        // These are vanilla + DLC ambient scenes that should be blockable when STFU_Scenes is enabled
        // Complete list of ~900 scene EditorIDs - checked against STFU::GetEditorID(topic)
        // NOTE: All stored in lowercase for case-insensitive matching
        std::vector<std::string> sceneNames = {
            "AddvarHouse2", "AddvarHouseScene1", "AncanoMirabelleScene01", "AngasMillCommonHouseScene01", "AngasMillCommonHouseScene02",
            "ApothecaryScene", "ArgonianScene1", "ArnielBrelynaScene01", "ArnielEnthirScene01", "ArnielEnthirScene02",
            "ArnielEnthirSceneDragon01", "ArnielNiryaScene01", "ArnielNiryaScene02", "AtWork", "BardsLunch",
            "BetridBoliMarketScene1", "BetridBoliMarketScene2", "BetridKerahMarketScene2", "BetridKerahScene1", "BirnaEnthirScene01",
            "BirnaEnthirScene02", "BlacksmithConversation", "BrelynaOnmundDormScene01", "BrelynaOnmundDormScene2", "BrinasHouseScene01",
            "BrinasHouseScene02", "BrylingFalkTryst", "BrylingScene1", "BrylingScene2", "BuyingARound",
            "CaravanScene1Scene", "CaravanScene2Scene", "CaravanScene3Scene", "CaravanScene4Scene", "CaravanScene5Scene",
            "CaravanScene6Scene", "CaravanScene7Scene", "CaravanScene8Scene", "CidhnaMinePrisoner01Scene", "CidhnaMinePrisoner03",
            "CidhnaMinePrisonerScene02", "CidhnaMinePrisonerScene04", "ColetteDrevisScene01", "CollegeGHallScene06", "CommanderScene1",
            "CommanderScene2", "CourtScene2", "DamphallMine01SceneQuestDialogue", "DawnstarIntroBrinaScene", "DGScene04",
            "DGSceneSpecial01", "DialogueBrandyMugFarmScene2", "DialogueBrandyMugFarmScene3", "DialogueCidhnaMineBlathlocGrisvar02Scene", "DialogueCompanionsAelaNjadaScene1",
            "DialogueCompanionsAelaNjadaScene2", "DialogueCompanionsFarkasAthisScene1", "DialogueCompanionsFarkasAthisScene2", "DialogueCompanionsFarkasTorvarScene1", "DialogueCompanionsFarkasTorvarScene2",
            "DialogueCompanionsKodlakAelaScene1", "DialogueCompanionsKodlakAelaScene2", "DialogueCompanionsKodlakFarkasScene1", "DialogueCompanionsKodlakFarkasScene2", "DialogueCompanionsKodlakSkjorScene1",
            "DialogueCompanionsKodlakSkjorScene2", "DialogueCompanionsKodlakTorvarScene1", "DialogueCompanionsKodlakTorvarScene2", "DialogueCompanionsRiaVilkasScene1", "DialogueCompanionsRiaVilkasScene2",
            "DialogueCompanionsRiaVilkasScene3", "DialogueCompanionsSkjorAelaScene1", "DialogueCompanionsSkjorAelaScene2", "DialogueCompanionsSkjorNjadaScene1", "DialogueCompanionsSkjorNjadaScene2",
            "DialogueCompanionsTorvarAthisScene1", "DialogueCompanionsTorvarAthisScene2", "DialogueDarkwaterCrossingAnnekeVernerScene1", "DialogueDarkwaterCrossingAnnekeVernerScene2", "DialogueDarkwaterCrossingAnnekeVernerScene3",
            "DialogueDarkwaterCrossingHrefna1", "DialogueDarkwaterCrossingHrefna2", "DialogueDarkwaterCrossingHrefna4", "DialogueDarkwaterCrossingInn4", "DialogueDarkwaterCrossingInn5",
            "DialogueDawnstarWindpeakInnScene04View", "DialogueDawnstarWindpeakInnScene05View", "DialogueDawnstarWindpeakInnScene06View", "DialogueDawnstarWindpeakInnScene07View", "DialogueDawnstarWindpeakInnScene08View",
            "DialogueDawnstarWindpeakInnScene09View", "DialogueDushnikhYalBlacksmithingScene01View", "DialogueDushnikhYalBlacksmithingScene02View", "DialogueDushnikhYalBlacksmithingScene03View", "DialogueDushnikhYalExterior01Scene",
            "DialogueDushnikhYalExterior02Scene", "DialogueDushnikhYalLonghouse01Scene", "DialogueDushnikhYalLonghouse02Scene", "DialogueDushnikhYalLonghouse03Scene", "DialogueDushnikhYalMine01Scene",
            "DialogueDushnikhYalMine02Scene", "DialogueGenericSceneDog01Scene", "DialogueHlaaluFarmScene1", "DialogueHlaaluFarmScene2", "DialogueHlaaluFarmScene3",
            "DialogueHollyfrostFarmTulvurHilleviScene1", "DialogueHollyfrostFarmTulvurHilleviScene2", "DialogueHollyfrostFarmTulvurTorstenScene1", "DialogueHollyfrostFarmTulvurTorstenScene2", "DialogueIvarsteadFellstarFarmScene01SCN",
            "DialogueIvarsteadFellstarFarmScene02SCN", "DialogueIvarsteadFellstarFarmScene03SCN", "DialogueIvarsteadInnScene01SCN", "DialogueIvarsteadInnScene02SCN", "DialogueIvarsteadInnScene03SCN",
            "DialogueIvarsteadInnScene04SCN", "DialogueIvarsteadInnScene05SCN", "DialogueIvarsteadInnScene06SCN", "DialogueIvarsteadKlimmekHouseScene01SCN", "DialogueIvarsteadKlimmekHouseScene02SCN",
            "DialogueIvarsteadTembasMillScene01SCN", "DialogueIvarsteadTembasMillScene02SCN", "DialogueKarthwastenMinersBarracksScene01View", "DialogueKarthwastenMinersBarracksScene02view", "DialogueKarthwastenMinersBarracksScene03View",
            "DialogueKarthwastenMinersBarracksScene04View", "DialogueKarthwastenMinersBarracksScene05View", "DialogueKarthwastenMinersBarracksScene06View", "DialogueKarthwastenMinersBarracksScene07View", "DialogueKarthwastenMinersBarracksScene08View",
            "DialogueKarthwastenMinersHouseScene03View", "DialogueKarthwastenOutsideScene01View", "DialogueKarthwastenOutsideScene02View", "DialogueKarthwastenSanuarachMineScene04View", "DialogueKarthwastenT01EnmonsHouseScene02View",
            "DialogueKarthwastenT01EnmonsHouseScene03View", "DialogueKarthwastenT01EnmonsHouseScene04View", "DialogueKolskeggrMineHouseScene02View", "DialogueKolskeggrMineScene01View", "DialogueKolskeggrMineScene02View",
            "DialogueKolskeggrMineScene03View", "DialogueKynesgroveGannaGemmaScene1", "DialogueKynesgroveGannaGemmaScene2", "DialogueKynesgroveGannaGemmaScene3", "DialogueKynesgroveGannaGemmaScene4",
            "DialogueKynesgroveGannaGemmaScene5", "DialogueKynesgroveGannaGemmaScene6", "DialogueKynesgroveKjeldDravyneaMineScene1", "DialogueKynesgroveKjeldDravyneaMineScene2", "DialogueKynesgroveKjeldDravyneaMineScene3",
            "DialogueKynesgroveKjeldYoungerIddraScene1", "DialogueKynesgroveKjeldYoungerIddraScene2", "DialogueKynesgroveKjeldYoungerIddraScene3", "DialogueKynesgroveKjeldYoungerKjeldScene1", "DialogueKynesgroveKjeldYoungerKjeldScene2",
            "DialogueKynesgroveKjeldYoungerKjeldScene3", "DialogueKynesgroveRoggiDravyneaMineScene1", "DialogueKynesgroveRoggiDravyneaMineScene2", "DialogueKynesgroveRoggiIddraScene1", "DialogueKynesgroveRoggiIddraScene2",
            "DialogueKynesgroveRoggiIddraScene3", "DialogueKynesgroveRoggiKjeldInnScene1", "DialogueKynesgroveRoggiKjeldInnScene2", "DialogueKynesgroveRoggiKjeldInnScene3", "DialogueKynesgroveRoggiKjeldScene1",
            "DialogueKynesgroveRoggiKjeldScene2", "DialogueKynesgroveRoggiKjeldScene3", "DialogueLeftHandMineDaighresHouse01Scene", "DialogueLeftHandMineDaighresHouse02View", "DialogueLeftHandMineDaighreSkaggi02Scene",
            "DialogueLeftHandMineMinersBarracks01Scene", "DialogueLeftHandMineMinersBarracks02View", "DialogueLeftHandMineScene04View", "DialogueMarkarthArnleifandSonsScene05View", "DialogueMarkarthArnleifandSonsScene06View",
            "DialogueMarkarthBlacksmithScene01View", "DialogueMarkarthDragonsKeepScene01View", "DialogueMarkarthDragonsKeepScene02View", "DialogueMarkarthDragonsMarketScene01View", "DialogueMarkarthDragonsRiversideScene01View",
            "DialogueMarkarthGenericScene01View", "DialogueMarkarthGenericScene02View", "DialogueMarkarthGenericScene03View", "DialogueMarkarthHagsCureSceneMuiriBothela03Scene", "DialogueMarkarthHagsCureSceneMuiriBothela04Scene",
            "DialogueMarkarthIntroArnleifandSonsSceneView", "DialogueMarkarthIntroSmelterSceneView", "DialogueMarkarthKeepIntroCourtSceneView", "DialogueMarkarthRiversideScene01View", "DialogueMarkarthRiversideScene02View",
            "DialogueMarkarthRiversideScene03View", "DialogueMarkarthRiversideScene04View", "DialogueMarkarthRiversideScene07View", "DialogueMarkarthRiversideScene08View", "DialogueMarkarthRiversideScene09View",
            "DialogueMarkarthRiversideScene10View", "DialogueMarkarthRiversideScene11View", "DialogueMarkarthRiversideScene12View", "DialogueMarkarthSilverFishInnScene16View", "DialogueMarkarthSilverFishInnScene17View",
            "DialogueMarkarthStablesBanningCedranScene01View", "DialogueMarkarthStablesBanningCedranScene02View", "DialogueMorKhazgurExterior02Scene", "DialogueMorKhazgurHuntingScene02View", "DialogueMorKhazgurLonghouse01Scene",
            "DialogueMorKhazgurLonghouse02Scene", "DialogueNarzulburBolarYatulScene1", "DialogueNarzulburBolarYatulScene2", "DialogueNarzulburBolarYatulScene3", "DialogueNarzulburBolarYatulScene4",
            "DialogueNarzulburMauhulakhBolarScene1", "DialogueNarzulburMauhulakhBolarScene2", "DialogueNarzulburMauhulakhDushnamubScene1", "DialogueNarzulburMauhulakhDushnamubScene2", "DialogueNarzulburMauhulakhDushnamubScene3",
            "DialogueNarzulburMauhulakhUrogScene1", "DialogueNarzulburMauhulakhUrogScene2", "DialogueNarzulburMauhulakhUrogScene3", "DialogueNarzulburMauhulakhYatulScene1", "DialogueNarzulburMauhulakhYatulScene2",
            "DialogueNarzulburMulGadbaScene1", "DialogueNarzulburMulGadbaScene2", "DialogueNarzulburMulGadbaScene3", "DialogueNarzulburMulGadbaScene4", "DialogueNarzulburUrogYatulScene1",
            "DialogueNarzulburUrogYatulScene2", "DialogueNarzulburUrogYatulScene3", "DialogueOldHroldanHangedManInnScene05View", "DialogueOldHroldanHangedManInnScene06View", "DialogueRiftenGrandPlazaScene019",
            "DialogueSalviusFarmSceneAView", "DialogueSalviusFarmSceneDView", "DialogueSalviusSceneCView", "DialogueSceneSigridAlvor", "DialogueSceneSigridEmbry",
            "DialogueSceneSigridHilde", "DialogueShorsStoneGatheringScene01SCN", "DialogueShorsStoneGatheringScene02SCN", "DialogueShorsStoneGatheringScene03SCN", "DialogueShorsStoneGatheringScene04SCN",
            "DialogueShorsStoneGatheringScene05SCN", "DialogueShorsStoneGatheringScene06SCN", "DialogueSolitudePalaceScene10ElisifBolgeir", "DialogueSolitudePalaceScene5FalkErikur", "DialogueSolitudePalaceScene6BrylingFalk",
            "DialogueSolitudePalaceScene7FalkBrylingSybille", "DialogueSolitudePalaceScene8ErikurFalkElisif", "DialogueSolitudePalaceScene9ElisifFalkBolgeir", "DialogueSoljundsMineHouseScene01View", "DialogueSoljundsMineHouseScene02View",
            "DialogueSoljundsMineHouseScene03View", "DialogueSoljundsMineHouseScene04View", "DialogueSoljundsMineScene01view", "DialogueSoljundsMineScene02view", "DialogueWhiterunAnoriathBrenuinScene1Scene",
            "DialogueWhiterunAnoriathOlfinaScene1Scene", "DialogueWhiterunAnoriathYsoldaScene1Scene", "DialogueWhiterunCarlottaBrenuinScene1Scene", "DialogueWhiterunCarlottaNazeemScene1View", "DialogueWhiterunCarlottaOlfinaScene1Scene",
            "DialogueWhiterunCarlottaYsoldaScene1Scene", "DialogueWhiterunDagnyFrothar1Scene", "DialogueWhiterunFraliaYsoldaScene1View", "DialogueWhiterunHrongarBalgruuf1Scene", "DialogueWhiterunHrongarProventus1Scene",
            "DialogueWhiterunHrongarProventus2Scene", "DialogueWhiterunIrilethBalgruuf1Scene", "DialogueWhiterunIrilethProventus1Scene", "DialogueWhiterunNazeemYsoldaScene1View", "DialogueWhiterunOlfinaYsoldaScene1View",
            "DialogueWhiterunProventusBalgruuf1Scene", "DialogueWhiterunSceneVignarBalgruuf1", "DialogueWhiterunTempleCastOnSoldier", "DialogueWhiterunTempleFarmerScene", "DialogueWhiterunTempleHealFarmerScene",
            "DialogueWhiterunTempleHealSoldierScene", "DialogueWhiterunTempleSoldierScene", "DialogueWhiterunTempleTalkFarmerScene", "DialogueWhiterunTempleTalkSoldierScene", "DialogueWhiterunYsoldaBrenuinScene1Scene",
            "DialogueWindhelmCandlehearthEldaCalixtoScene7", "DialogueWindhelmCandlehearthEldaLonelyGaleScene6", "DialogueWindhelmCandlehearthHallScene1", "DialogueWindhelmCandlehearthHallScene2", "DialogueWindhelmCandlehearthHallScene3",
            "DialogueWindhelmEldaLonelyGaleScene1", "DialogueWindhelmMarketSceneBrunwulfAvalScene", "DialogueWindhelmMarketSceneJovaNiranyeScene", "DialogueWindhelmMarketSceneLonelyGaleNiranyeScene", "DialogueWindhelmMarketSceneNilsineHilleviScene",
            "DialogueWindhelmMarketSceneTorbjornHilleviScene", "DialogueWindhelmMarketSceneTorbjornNiranyeScene", "DialogueWindhelmMarketSceneTorstenHilleviScene", "DialogueWindhelmMarketSceneTorstenNiranyeScene", "DialogueWindhelmMarketSceneTovaAvalScene",
            "DialogueWindhelmNurelionQuintusScene1", "DialogueWindhelmNurelionQuintusScene2", "DialogueWindhelmNurelionQuintusScene3", "DialogueWindhelmPalaceUlfricGormlaithScene9", "DialogueWindhelmRevynNiranyeScene1",
            "DialogueWindhelmUlfricGormlaithScene1", "DialogueWindhelmUlfricTorstenScene1", "DialogueWindhelmViolaBothersLonelyGaleScene1", "DialogueWindhelmViolaBothersLonelyGaleScene2", "DialogueWindhelmViolaBothersLonelyGaleScene3",
            "DialogueWindhelmViolaBothersLonelyGaleScene4", "DialogueWinterholdBirnasHouseScene1", "DialogueWinterholdBirnasHouseScene2", "DialogueWinterholdInnInitialScene", "DLC1HunterBaseScene1",
            "DLC1HunterBaseScene10", "DLC1HunterBaseScene2", "DLC1HunterBaseScene3", "DLC1HunterBaseScene4", "DLC1HunterBaseScene5",
            "DLC1HunterBaseScene6", "DLC1HunterBaseScene7", "DLC1HunterBaseScene8", "DLC1HunterBaseScene9", "DLC1VampireBaseScene01",
            "DLC1VampireBaseScene02", "DLC1VampireBaseScene03", "DLC1VampireBaseScene04", "DLC1VampireBaseScene05", "DLC1VampireBaseScene06",
            "DLC2DrovasElyneaScene01", "DLC2DrovasElyneaScene02", "DLC2DrovasNelothScene01", "DLC2DrovasNelothScene02", "DLC2DrovasTalvasScene01",
            "DLC2DrovasTalvasScene02", "DLC2MHBujoldElmusReclaimScene01Scene", "DLC2MHBujoldElmusScene01Scene", "DLC2MHBujoldHilundReclaimScene01Scene", "DLC2MHBujoldHilundScene01",
            "DLC2MHBujoldKuvarReclaimScene01Scene", "DLC2MHBujoldKuvarReclaimScene02Scene", "DLC2MHBujoldKuvarReclaimScene03Scene", "DLC2MHBujoldKuvarScene01Scene", "DLC2MHBujoldKuvarScene02Scene",
            "DLC2MHBujoldKuvarScene03Scene", "DLC2MHElmusKuvarReclaimScene01Scene", "DLC2MHElmusKuvarScene01Scene", "DLC2MHElmusKuvarScene02Scene", "DLC2MHKuvarHilundReclaimScene01SCene",
            "DLC2MHKuvarHilundScene01Scene", "DLC2MQ02FreaTempleScene", "DLC2RRAlorHouseScene001", "DLC2RRAlorHouseScene002", "DLC2RRAnyLocScene001",
            "DLC2RRAnyLocScene0016", "DLC2RRAnyLocScene002", "DLC2RRAnyLocScene003", "DLC2RRAnyLocScene004", "DLC2RRAnyLocScene005",
            "DLC2RRAnyLocScene006", "DLC2RRAnyLocScene007", "DLC2RRAnyLocScene008", "DLC2RRAnyLocScene009", "DLC2RRAnyLocScene010",
            "DLC2RRAnyLocScene011", "DLC2RRAnyLocScene012", "DLC2RRAnyLocScene013", "DLC2RRAnyLocScene014", "DLC2RRAnyLocScene015",
            "DLC2RRAnyLocScene017", "DLC2RRAnyLocScene018", "DLC2RRAnyLocScene019", "DLC2RRAnyLocScene020", "DLC2RRAnyLocScene021",
            "DLC2RRBeggarBralsaScene01", "DLC2RRCresciusHouseScene001", "DLC2RRCresciusHouseScene002", "DLC2RRIenthFarmScene002", "DLC2RRMorvaynManorScene001",
            "DLC2RRMorvaynManorScene002", "DLC2RRMorvaynManorScene003", "DLC2RRMorvaynManorScene004", "DLC2RRMorvaynManorScene005", "DLC2RRMorvaynManorScene006",
            "DLC2RRNetchScene001", "DLC2RRNetchScene002", "DLC2RRNetchScene003", "DLC2RRNetchScene004", "DLC2RRNetchScene005",
            "DLC2RRNetchScene006", "DLC2RRNetchScene007", "DLC2RRNetchScene008", "DLC2RRNetchScene009", "DLC2RRNetchScene010",
            "DLC2RROthrelothPreachingScene00", "DLC2RRSeverinManorScene001", "DLC2RRSeverinManorScene002", "DLC2RRSeverinManorScene003", "DLC2RRTempleScene001",
            "DLC2RRTempleScene002", "DLC2RRTempleScene003", "DLC2SVBaldorMorwenScene01Scene", "DLC2SVBaldorMorwenScene02Scene", "DLC2SVBaldorMorwenScene03Scene",
            "DLC2SVDeorWulfScene01Scene", "DLC2SVDeorWulfScene02Scene", "DLC2SVDeorWulfScene03Scene", "DLC2SVDeorYrsaScene01Scene", "DLC2SVDeorYrsaScene02Scene",
            "DLC2SVEdlaNikulasScene01Scene", "DLC2SVMorwenTharstanScene01Scene", "DLC2SVOslafAetaScene01Scene", "DLC2SVOslafFinnaScene01Scene", "DLC2SVOslafFinnaScene02Scene",
            "DLC2SVOslafYrsaScene01Scene", "DLC2SVTharstanFanariScene01Scene", "DLC2SVTharstanFanariScene02Scene", "DLC2TelMithrynNelothTalvas01", "DLC2TelMithrynNelothTalvas02",
            "DLC2TelMithrynNelothTalvas03", "DLC2TelMithrynNelothTalvas04", "DLC2VaronaElyneaScene01", "DLC2VaronaElyneaScene02", "DLC2VaronaNelothScene01",
            "DLC2VaronaNelothScene02", "DLC2VaronaTalvasScene01", "DLC2VaronaTalvasScene02", "DLC2VaronaUlvesScene01", "DLC2VaronaUlvesScene02",
            "DragonBridgeFarmScene01", "DragonBridgeFarmScene02", "DragonBridgeHorgeirsHouseScene01", "DragonBridgeMillSceneHorgeirLodvar", "DragonBridgeMillSceneLodvarOlda",
            "DragonBridgeScene01", "DragonBridgeTavernSceneFaidaJuli", "DragonBridgeTavernSceneJuliFaida", "DrinkingContest", "EasternMineScene01",
            "EasternMineScene02", "EndonKerahMarketScene1", "EndonKerahMarketScene3", "ErikurFamilyScene", "ErikurFamilyScene2",
            "ErikurMelaranScene", "FalkElisifAboutPower", "FalkreathCemeteryScene01", "FalkreathCemeteryScene02", "FalkreathCorpselightFarmScene01",
            "FalkreathDeadMansDrinkScene01", "FalkreathDeadMansDrinkScene02", "FalkreathDeadMansDrinkScene03", "FalkreathDeadMansDrinkScene04", "FalkreathDeadMansDrinkScene05",
            "FalkreathDeadMansDrinkScene06", "FalkreathDeadMansDrinkScene07", "FalkreathDeadMansDrinkScene08", "FalkreathDeadMansDrinkScene09", "FalkreathDeadMansDrinkScene10",
            "FalkreathDeadMansDrinkScene11", "FalkreathDengeirsHallScene01", "FalkreathDengeirsHallScene02", "FalkreathDengeirsHallScene03", "FalkreathGrayPineGoodsScene01",
            "FalkreathHouseofArkayScene01", "FalkreathHouseofArkayScene02", "FalkreathHouseofArkayScene03", "FalkreathJarlsLonghouseScene01", "FalkreathJarlsLonghouseScene02",
            "FalkreathJarlsLonghouseScene03", "FalkSybille", "FaraldaEnthirScene01", "FletcherScene", "FletcherScene2",
            "GretaLookingForWork", "HeartwoodMillScene01", "HeartwoodMillScene02", "HighmoonHallScene01", "HighmoonHallScene02",
            "HighmoonHallScene03", "HighmoonHallScene04", "HighmoonHallScene05", "HighmoonHallScene06", "JailerScene2",
            "JailScene1", "JornAiaScene", "JzargoOnmundDormScene01", "KatlaFamilyScene", "KidsPlaying",
            "LoreiusFarmOutsideScene01", "LoreiusFarmOutsideScene02", "MarkarthArnleifandSonsScene02", "MarkarthEndonsHouseScene01", "MarkarthEndonsHouseScene02",
            "MarkarthHagsCureScene01", "MarkarthHagsCureScene02", "MarkarthKeepScene01", "MarkarthKeepScene02", "MarkarthKeepScene03",
            "MarkarthKeepScene04", "MarkarthKeepScene05", "MarkarthKeepScene06", "MarkarthKeepScene08", "MarkarthKeepScene10",
            "MarkarthKeepScene10DUPLICATE001", "MarkarthSilverFishhInnScene13", "MarkarthSilverFishInnScene01", "MarkarthSilverFishInnScene02", "MarkarthSilverFishInnScene03",
            "MarkarthSilverFishInnScene04", "MarkarthSilverFishInnScene05", "MarkarthSilverFishInnScene06", "MarkarthSilverFishInnScene12", "MarkarthSilverFishInnScene14",
            "MarkarthTreasuryHouseScene1", "MarkarthTreasuryHouseScene10", "MarkarthTreasuryHouseScene2", "MarkarthTreasuryHouseScene3", "MarkarthTreasuryHouseScene6",
            "MarkarthTreasuryHouseScene7", "MarkarthTreasuryHouseScene8", "MarkarthWarrensScene01", "MarkarthWarrensScene03", "MarkarthWarrensScene04",
            "MarkarthWizardsTowerScene01", "MarkarthWizardsTowerScene02", "MerryfairFarmScene02", "MerryfairScene01", "MjollsHouseScene01",
            "MoorsideScene05", "MoorsideScene06", "MorthalAlvasHouseScene1", "MorthalFalionsHouseScene1", "MorthalFalionsHouseScene2",
            "MorthalFalionsHouseScene3", "MorthalFalionsHouseScene4", "MorthalMoorsideScene07", "MorthalMoorsideScene08", "MorthalMoorsideScene1",
            "MorthalMoorsideScene2", "MorthalMoorsideScene3", "MorthalMoorsideScene4", "MorthalThaumaturgistHutScene01", "MorthalThaumaturgistsHutScene2",
            "MorthalThaumaturgistsHutScene3", "NosterBegging", "PalaceScene1", "RaimentsScene", "RiftenBBManorScene01",
            "RiftenBBManorScene02", "RiftenBBMeaderyScene01", "RiftenBBMeaderyScene02", "RiftenBBMeaderyScene03", "RiftenBeeAndBarbScene01",
            "RiftenBeeAndBarbScene02", "RiftenBeeAndBarbScene03", "RiftenBeeAndBarbScene04", "RiftenBeeAndBarbScene05", "RiftenBeeAndBarbScene06",
            "RiftenBeeAndBarbScene07", "RiftenBeeAndBarbScene08", "RiftenBeeAndBarbScene09", "RiftenBeeAndBarbScene10", "RiftenBeeAndBarbScene11",
            "RiftenBeeAndBarbScene12", "RiftenBeeandBarbScene13", "RiftenBeeAndBarbScene14", "RiftenBeeAndBarbScene15", "RiftenBeeAndBarbScene16",
            "RiftenBeeAndBarbScene17", "RiftenBeeAndBarbScene18", "RiftenBeeAndBarbScene19", "RiftenBeeandBarbScene20", "RiftenBeggarEddaScene",
            "RiftenBeggarSnilfScene", "RiftenElgrimsElixirsScene01", "RiftenElgrimsElixirsScene02", "RiftenElgrimsElixirsScene03", "RiftenElgrimsElixirsScene4",
            "RiftenFisheryScene01", "RiftenFisheryScene02", "RiftenFisheryScene03", "RiftenFisheryScene04", "RiftenGrandPlazaScene01",
            "RiftenGrandPlazaScene02", "RiftenGrandPlazaScene03", "RiftenGrandPlazaScene04", "RiftenGrandPlazaScene05", "RiftenGrandPlazaScene06",
            "RiftenGrandPlazaScene07", "RiftenGrandPlazaScene08", "RiftenGrandPlazaScene09", "RiftenGrandPlazaScene10", "RiftenGrandPlazaScene11",
            "RiftenGrandPlazaScene12", "RiftenGrandPlazaScene13", "RiftenGrandPlazaScene14", "RiftenGrandPlazaScene15", "RiftenGrandPlazaScene16",
            "RiftenGrandPlazaScene17", "RiftenGrandPlazaScene18", "RiftenGrandPlazaScene20", "RiftenGrandPlazaScene21", "RiftenGrandPlazaScene22",
            "RiftenGrandPlazaScene23", "RiftenGrandPlazaScene24", "RiftenGrandPlazaScene25", "RiftenGrandPlazaScene26", "RiftenGrandPlazaScene27",
            "RiftenGrandPlazaScene28", "RiftenGrandPlazaScene29", "RiftenGrandPlazaScene30", "RiftenGrandPlazaScene31", "RiftenGrandPlazaScene32",
            "RiftenGrandPlazaScene33", "RiftenGrandPlazaScene34", "RiftenGrandPlazaScene35", "RiftenGrandPlazaScene36", "RiftenGrandPlazaScene37",
            "RiftenGrandPlazaScene38", "RiftenGrandPlazaScene39", "RiftenGrandPlazaScene40", "RiftenGrandPlazaScene41", "RiftenGrandPlazaScene42",
            "RiftenGrandPlazaScene43", "RiftenHaelgasBunkhouseScene01", "RiftenHaelgasBunkhouseScene02", "RiftenHaelgasBunkhouseScene03", "RiftenHaelgasBunkhouseScene04",
            "RiftenHaelgasBunkhouseScene05", "RiftenHaelgasBunkhouseScene06", "RiftenHaelgasBunkhouseScene07", "RiftenHaelgasBunkhouseScene08", "RiftenHaelgasBunkhouseScene09",
            "RiftenHaelgasBunkhouseScene10", "RiftenHaelgasBunkhouseScene11", "RiftenHaelgasBunkhouseScene12", "RiftenHonorhallScene01", "RiftenKeepScene01",
            "RiftenKeepScene01Alternate", "RiftenKeepScene02", "RiftenKeepScene02Alternate", "RiftenKeepScene03", "RiftenKeepScene03Alternate",
            "RiftenKeepScene04Alternate", "RiftenKeepScene05", "RiftenKeepScene05Alternate", "RiftenKeepScene06", "RiftenKeepScene06Alternate",
            "RiftenKeepScene07", "RiftenKeepScene07Alternate", "RiftenKeepScene08", "RiftenKeepScene08Alternate01", "RiftenKeepScene09",
            "RiftenKeepScene10", "RiftenKeepScene11", "RiftenMjollHouseScene02", "RiftenPawnedPrawnScene01", "RiftenPawnedPrawnScene02",
            "RiftenPawnedPrawnScene03", "RiftenRaggedFlagon05Scene", "RiftenRaggedFlagonScene01", "RiftenRaggedFlagonScene02", "RiftenRaggedFlagonScene03",
            "RiftenRaggedFlagonScene04", "RiftenRaggedFlagonScene06", "RiftenRaggedFlagonScene07", "RiftenRaggedFlagonScene08", "RiftenRaggedFlagonScene09",
            "RiftenRaggedFlagonScene10", "RiftenRaggedFlagonScene11", "RiftenRaggedFlagonScene12", "RiftenSnowShodHouseScene01", "RiftenSnowShodHouseScene02",
            "RiftenSnowShodHouseScene03", "RiftenTempleofMaraScene01", "RiftenTempleofMaraScene02", "RiverwoodAlvorDortheScene2", "RiverwoodFrodnarHodScene1",
            "RiverwoodSceneA", "RiverwoodSceneEmbrySoldierScene", "RiverwoodSceneSigridAlvorCabbage", "RiverwoodSceneSoldierHildeScene", "RiverwoodSigridDortheScene1",
            "RiverwoodSleepingGiantOrderDrinks", "RiverwoodSleepingGiantScene3", "RiverwoodSleepingGiantScene4", "RiverwoodSleepingGiantScene5", "RiverwoodSleepingGiantScene6",
            "Romance", "RoriksteadBritteSisselScene1", "RoriksteadEnnisReldithScene1", "RoriksteadSisselRouaneScene1", "RustleifSmithyScene01",
            "RustleifSmithyScene02", "SalviusFarmSceneAView", "SanHouse2", "SanHouseScene1", "SarethiFarmScene01",
            "SarethiFarmScene02", "SarethiFarmScene03", "SavosMirabelleScene01", "SavosMirabelleScene02", "SavosMirabelleScene03",
            "SavosMirabelleScene04", "SkyHavenTempleConversation02", "SkyHavenTempleConversation03", "SkyHavenTempleConversation04", "SkyHavenTempleConversationScene",
            "SleepingGiantDelphineOrgnarScene", "SleepingGiantDelphineRaevildScene", "SleepingGiantScene7", "SleepingGiantScene8", "SnowShodFarmScene01",
            "SnowShodFarmScene02", "SolitudeBardScene4", "SolitudeBardScene5", "SolitudeBardScene6", "SolitudeMarketplaceAiaEvetteScene1",
            "SolitudeMarketplaceAtAfAlanAdvarScene1", "SolitudeMarketplaceAtAfAlanEvetteScene1", "SolitudeMarketplaceAtAfAlanJalaScene1", "SolitudeMarketplaceIlldiAdvarScene1", "SolitudeMarketplaceIlldiEvetteScene1",
            "SolitudeMarketplaceIlldiJalaScene1", "SolitudeMarketplaceJawananAdvarScene1", "SolitudeMarketplaceJawananEvetteScene1", "SolitudeMarketplaceJawananJalaScene", "SolitudeMarketplaceJornAdvarScene1",
            "SolitudeMarketplaceJornJalaScene1", "SolitudeMarketplaceRorlundAdvarScene1", "SolitudeMarketplaceRorlundEvetteScene1", "SolitudeMarketplaceRorlundJalaScene1", "SolitudeMarketplaceSilanaAdvarScene1",
            "SolitudeMarketplaceSilanaEvetteScene1", "SolitudeMarketplaceSilanaJalaScene1", "SolitudeMarketplaceSorexAdvarScene1", "SolitudeMarketplaceSorexEvetteScene1", "SolitudeMarketplaceSorexJalaScene1",
            "SolitudeMarketplaceXanderAngelineScene1", "SolitudeMarketplaceXanderSaymaScene1", "SolitudeMarketplaceXanderTaarieScene1", "SolitudeSawmillScene", "SolitudeStreetOdarJawananScene1",
            "SolitudeStreetOdarJornScene1", "SolitudeStreetUnaJornScene1", "SolitudeStreetUnaLisetteScene1", "SolitudeStreetVivienneLisetteScene", "SolitudeStreetVivienneLisetteScene2",
            "SolitudeStreetVivienneLisetteScene3", "StonehillsGesturJesperScene1", "StonehillsGesturSwanhvirScene1", "StonehillsGesturTeebaEiScene1", "StonehillsMineGesturTalibScene1",
            "StudentTeacher", "TeachersScene", "TempleScene1", "TempleScene2", "TempleScene3",
            "UragColetteScene01", "UragColetteScene02", "UragDrevisScene02", "UragNiryaScene01", "UragNiryaScene02",
            "UragSergiusScene01", "ViniusFamilyScene", "ViniusFamilyScene2", "WarehouseWorkScene", "WesternMineScene01",
            "WesternMineScene02", "WhiteHallImperialScene01", "WhiteHallImperialScene02", "WhiteHallImperialScene03", "WhiteHallImperialScene04",
            "WhiteHallImperialScene05", "WhiteHallSonsScene01", "WhiteHallSonsScene02", "WhiteHallSonsScene03", "WhiteHallSonsScene04",
            "WhiteHallSonsScene05", "WhiteHallsonsScene06", "WhiterunAnoriathElrindirScene1", "WhiterunAnoriathElrindirScene2", "WhiterunAnoriathElrindirScene3",
            "WhiterunBanneredMareScene1", "WhiterunBanneredMareScene2", "WhiterunBanneredMareScene3", "WhiterunBraithAmrenScene1", "WhiterunBraithAmrenScene2",
            "WhiterunBraithLarsScene1", "WhiterunBraithLarsScene2", "WhiterunBraithLarsScene3", "WhiterunBraithSaffirScene1", "WhiterunBraithSaffirScene2",
            "WhiterunDragonsreachScene1", "WhiterunHeimskrPreachScene", "WhiterunHouseBattleBornScene1", "WhiterunHouseBattleBornScene2", "WhiterunHouseGrayManeScene1",
            "WhiterunHouseGrayManeScene2", "WhiterunMarketplaceScene1", "WhiterunMarketplaceScene2", "WhiterunMarketplaceScene3", "WhiterunMarketplaceScene4a",
            "WhiterunOlfinaJonScene1", "WhiterunOlfinaJonScene2", "WhiterunOlfinaJonScene3", "WhiterunStablesScene1", "WhiterunStablesScene4",
            "WhiterunTempleOfKynarethScene1", "WhiterunVignarBrillScene1", "WhiterunVignarBrillScene2", "WhiterunVignarBrillScene3", "WIGreetingScene",
            "WindhelmDialogueAmbaryScoutsManyMarshesScene1", "WindhelmDialogueAmbarysScoutsManyMarshesScene2", "WindhelmDialogueAmbarySuvarisScene1", "WindhelmDialogueAmbarySuvarisScene2", "WindhelmDialogueEldaCalixtoScene1",
            "WindhelmDialogueEldaJoraScene1", "WindhelmDialogueEldaJoraScene2", "WindhelmDialogueUlfricGormlaith2", "WindhelmDialogueUlfricJorleifScene1", "WindhelmDialogueUlfricJorleifScene2",
            "WindhelmDialogueUlfricLonelyGaleScene1", "WindhelmDialogueUlfricLonelyGaleScene2", "WindhelmOengulHermirScene3", "WindhelmUlfricJorleifScene3", "WindpeakInnScene02",
            "WindpeakInnScene03", "WinterholdInnScene01", "WinterholdInnScene02", "WinterholdInnScene03", "WinterholdInnScene04",
            "WinterholdInnScene05", "WinterholdInnScene06", "WinterholdInnScene07", "WinterholdInnScene08", "WinterholdJarlsLonghouseScene01",
            "WinterholdJarlsLonghouseScene02", "WinterholdJarlsLonghouseScene03", "WinterholdKorirsHouseScene01", "WinterholdKorirsHouseScene02", "WinterholdKorirsHouseScene03",
            "WIRentRoomWalkToScene", "WITavernGreeting", "WITavernPlayerSits"
        };
        
        // Store scene names with proper casing
        for (const auto& name : sceneNames) {
            g_settings.hardcodedScenes.topicEditorIDs.insert(name);
        }
        
        spdlog::info("Initialized {} hardcoded ambient scenes", g_settings.hardcodedScenes.topicEditorIDs.size());
    }
    
    bool IsBardSongQuest(RE::TESQuest* quest)
    {
        if (!quest) {
            return false;
        }
        
        const char* questEditorID = STFU::GetEditorID(quest);
        if (!questEditorID) {
            return false;
        }
        
        // Check hardcoded list (only 3 quests)
        static const std::unordered_set<std::string> bardSongQuests = {
            "BardSongs",
            "BardSongsInstrumental",
            "MS05BardSongs"
        };
        
        return bardSongQuests.count(questEditorID) > 0;
    }
    
    bool IsHardcodedAmbientScene(RE::TESTopic* topic)
    {
        if (!topic) {
            return false;
        }
        
        const char* topicEditorID = STFU::GetEditorID(topic);
        if (!topicEditorID) {
            return false;
        }
        
        // For scene topics, need to find the scene EditorID to check blacklist
        // (user blacklists by scene EditorID, not topic EditorID)
        auto quest = topic->ownerQuest;
        if (quest) {
            auto& scenesArray = quest->scenes;
            
            for (auto* scene : scenesArray) {
                if (!scene) continue;
                for (auto* action : scene->actions) {
                    if (!action || action->GetType() != RE::BGSSceneAction::Type::kDialogue) continue;
                    auto* dialogueAction = static_cast<RE::BGSSceneActionDialogue*>(action);
                    if (dialogueAction && dialogueAction->topic == topic) {
                        const char* sceneEditorID = STFU::GetEditorID(scene);
                        if (sceneEditorID) {
                            // Actor filtering not available at this level (no TESTopicInfo)
                            if (DialogueDB::GetDatabase()->ShouldSoftBlock(0, sceneEditorID)) {
                                return true;
                            }
                        }
                        break;
                    }
                }
            }
        }
        
        // Fallback to in-memory hardcoded list (case-sensitive, database handles most lookups)
        bool found = g_settings.hardcodedScenes.topicEditorIDs.count(topicEditorID) > 0;
        
        if (found || g_settings.hardcodedScenes.topicEditorIDs.empty()) {
            spdlog::info("[HARDCODED CHECK] Topic '{}' - Found: {}, List size: {}", 
                topicEditorID, found, g_settings.hardcodedScenes.topicEditorIDs.size());
        }
        
        return found;
    }
    
    bool IsHardcodedAmbientScene(RE::BGSScene* scene)
    {
        if (!scene) {
            return false;
        }
        
        const char* sceneEditorID = STFU::GetEditorID(scene);
        if (!sceneEditorID) {
            return false;
        }
        
        // Check unified blacklist (scenes have target_type=4)
        // Note: Actor filtering not applicable for scene-level checks
        if (DialogueDB::GetDatabase()->ShouldSoftBlock(0, sceneEditorID)) {
            return true;
        }
        
        // Fallback to in-memory hardcoded list (case-sensitive, database handles most lookups)
        return g_settings.hardcodedScenes.topicEditorIDs.count(sceneEditorID) > 0;
    }

    RE::TESGlobal* GetSceneGateGlobalForCategory(const std::string& filterCategory)
    {
        // Which toggle controls this scene's hard-block depends on how it was
        // categorized in the blacklist. User-added entries ("Blacklist") must
        // follow the master blacklist toggle, not the pre-included scenes toggle.
        if (filterCategory == "Blacklist")           return GetBlacklistGlobal();
        if (filterCategory == "FollowerCommentary")  return GetFollowerCommentaryGlobal();
        if (filterCategory == "BardSongs")           return GetBardSongsGlobal();
        // "Scene" and any unknown category default to the pre-included scenes toggle.
        return GetScenesGlobal();
    }

    std::vector<std::string> GetHardcodedScenesList()
    {
        // Return scene names from the in-memory set (now stored with correct casing)
        std::vector<std::string> scenes;
        for (const auto& scene : g_settings.hardcodedScenes.topicEditorIDs) {
            scenes.push_back(scene);
        }
        return scenes;
    }
    
    std::vector<std::string> GetBardSongQuestsList()
    {
        return {"BardSongs", "BardSongsInstrumental", "MS05BardSongs"};
    }
    
    std::vector<std::string> GetFollowerCommentaryScenesList()
    {
        return {"WIFollowerChatter02Scene", "WIFollowerChatter03Scene"};
    }
}  // namespace Config
