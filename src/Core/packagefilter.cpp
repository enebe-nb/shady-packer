#include "package.hpp"

#include <boost/filesystem.hpp>

static bool FilterFromZipTextFileExtension(ShadyCore::PackageFilter& filter, ShadyCore::BasePackageEntry& entry) {
	if (entry.getType() != ShadyCore::BasePackageEntry::TYPE_ZIP) return false;
	const ShadyCore::FileType& type = ShadyCore::FileType::get(entry);

	if (type.isEncrypted && type == ShadyCore::FileType::TYPE_TEXT) {
		boost::filesystem::path path(entry.getName());
		path.replace_extension(type.inverseExt);
		return filter.renameEntry(path.generic_string().c_str());
	} return false;
}

static bool FilterToZipTextFileExtension(ShadyCore::PackageFilter& filter, ShadyCore::BasePackageEntry& entry) {
	const ShadyCore::FileType& type = ShadyCore::FileType::get(entry);

	if (!type.isEncrypted && type == ShadyCore::FileType::TYPE_TEXT) {
		boost::filesystem::path path(entry.getName());
		path.replace_extension(type.inverseExt);
		return filter.renameEntry(path.generic_string().c_str());
	} return false;
}

static bool FilterToZipConvert(ShadyCore::PackageFilter& filter, ShadyCore::BasePackageEntry& entry) {
	const ShadyCore::FileType& type = ShadyCore::FileType::get(entry);

	if (type.isEncrypted && type == ShadyCore::FileType::TYPE_IMAGE
		|| !type.isEncrypted && type == ShadyCore::FileType::TYPE_SFX
		|| !type.isEncrypted && type == ShadyCore::FileType::TYPE_PALETTE
		|| !type.isEncrypted && type == ShadyCore::FileType::TYPE_GUI
		|| !type.isEncrypted && type == ShadyCore::FileType::TYPE_LABEL
		|| !type.isEncrypted && type == ShadyCore::FileType::TYPE_PATTERN) {

		return filter.convertEntry(type);
	}
	else if (type.isEncrypted && type == ShadyCore::FileType::TYPE_TEXT) {
		return filter.convertData(type);
	}

	return false;
}

static bool FilterDecryptAll(ShadyCore::PackageFilter& filter, ShadyCore::BasePackageEntry& entry) {
	const ShadyCore::FileType& type = ShadyCore::FileType::get(entry);

	if (type == ShadyCore::FileType::TYPE_UNKNOWN
		|| type == ShadyCore::FileType::TYPE_PACKAGE
		|| !type.isEncrypted) return false;

	return filter.convertEntry(type);
}

static bool FilterEncryptAll(ShadyCore::PackageFilter& filter, ShadyCore::BasePackageEntry& entry) {
	const ShadyCore::FileType& type = ShadyCore::FileType::get(entry);

	if (type == ShadyCore::FileType::TYPE_UNKNOWN
		|| type == ShadyCore::FileType::TYPE_PACKAGE
		|| type.isEncrypted) return false;

	return filter.convertEntry(type);
}

static bool FilterSlashToUnderline(ShadyCore::PackageFilter& filter, ShadyCore::BasePackageEntry& entry) {
	const char* oldName = entry.getName();
	char newName[256];
	bool replaced = false;
	for (int i = 0; newName[i] = oldName[i]; ++i) {
		if (oldName[i] == '/') {
			replaced = true;
			newName[i] = '_';
		}
	}

	if (replaced) return filter.renameEntry(newName);
	return false;
}

static bool FilterUnderlineToSlash(ShadyCore::PackageFilter& filter, ShadyCore::BasePackageEntry& entry) {
	const char* dirTable[][2] = {
		{ "data_weather_effect_", "data/weather/effect/" },
		{ "data_weather_", "data/weather/" },
		{ "data_stand_cutin_", "data/stand/cutin/" },
		{ "data_stand_", "data/stand/" },
		{ "data_se_yuyuko_", "data/se/yuyuko/" },
		{ "data_se_yukari_", "data/se/yukari/" },
		{ "data_se_youmu_", "data/se/youmu/" },
		{ "data_se_utsuho_", "data/se/utsuho/" },
		{ "data_se_udonge_", "data/se/udonge/" },
		{ "data_se_tenshi_", "data/se/tenshi/" },
		{ "data_se_suwako_", "data/se/suwako/" },
		{ "data_se_suika_", "data/se/suika/" },
		{ "data_se_sanae_", "data/se/sanae/" },
		{ "data_se_sakuya_", "data/se/sakuya/" },
		{ "data_se_remilia_", "data/se/remilia/" },
		{ "data_se_reimu_", "data/se/reimu/" },
		{ "data_se_patchouli_", "data/se/patchouli/" },
		{ "data_se_namazu_", "data/se/namazu/" },
		{ "data_se_meirin_", "data/se/meirin/" },
		{ "data_se_marisa_", "data/se/marisa/" },
		{ "data_se_komachi_", "data/se/komachi/" },
		{ "data_se_iku_", "data/se/iku/" },
		{ "data_se_chirno_", "data/se/chirno/" },
		{ "data_se_aya_", "data/se/aya/" },
		{ "data_se_alice_", "data/se/alice/" },
		{ "data_se_", "data/se/" },
		{ "data_scene_title_", "data/scene/title/" },
		{ "data_scene_staffroll_moji_", "data/scene/staffroll/moji/" },
		{ "data_scene_staffroll_", "data/scene/staffroll/" },
		{ "data_scene_select_scenario_", "data/scene/select/scenario/" },
		{ "data_scene_select_character_yuyuko_", "data/scene/select/character/yuyuko/" },
		{ "data_scene_select_character_yukari_", "data/scene/select/character/yukari/" },
		{ "data_scene_select_character_youmu_", "data/scene/select/character/youmu/" },
		{ "data_scene_select_character_utsuho_", "data/scene/select/character/utsuho/" },
		{ "data_scene_select_character_udonge_", "data/scene/select/character/udonge/" },
		{ "data_scene_select_character_tenshi_", "data/scene/select/character/tenshi/" },
		{ "data_scene_select_character_suwako_", "data/scene/select/character/suwako/" },
		{ "data_scene_select_character_suika_", "data/scene/select/character/suika/" },
		{ "data_scene_select_character_sanae_", "data/scene/select/character/sanae/" },
		{ "data_scene_select_character_sakuya_", "data/scene/select/character/sakuya/" },
		{ "data_scene_select_character_remilia_", "data/scene/select/character/remilia/" },
		{ "data_scene_select_character_reimu_", "data/scene/select/character/reimu/" },
		{ "data_scene_select_character_patchouli_", "data/scene/select/character/patchouli/" },
		{ "data_scene_select_character_meirin_", "data/scene/select/character/meirin/" },
		{ "data_scene_select_character_marisa_", "data/scene/select/character/marisa/" },
		{ "data_scene_select_character_komachi_", "data/scene/select/character/komachi/" },
		{ "data_scene_select_character_iku_", "data/scene/select/character/iku/" },
		{ "data_scene_select_character_chirno_", "data/scene/select/character/chirno/" },
		{ "data_scene_select_character_aya_", "data/scene/select/character/aya/" },
		{ "data_scene_select_character_alice_", "data/scene/select/character/alice/" },
		{ "data_scene_select_character_09b_character_", "data/scene/select/character/09b_character/" },
		{ "data_scene_select_character_08b_circle_", "data/scene/select/character/08b_circle/" },
		{ "data_scene_select_character_05a_selcha_", "data/scene/select/character/05a_selcha/" },
		{ "data_scene_select_character_01b_name_", "data/scene/select/character/01b_name/" },
		{ "data_scene_select_character_", "data/scene/select/character/" },
		{ "data_scene_select_bg_", "data/scene/select/bg/" },
		{ "data_scene_select_", "data/scene/select/" },
		{ "data_scene_opening_yuki_", "data/scene/opening/yuki/" },
		{ "data_scene_opening_ame_", "data/scene/opening/ame/" },
		{ "data_scene_opening_", "data/scene/opening/" },
		{ "data_scene_logo_", "data/scene/logo/" },
		{ "data_scene_", "data/scene/" },
		{ "data_scenario_yuyuko_effect_", "data/scenario/yuyuko/effect/" },
		{ "data_scenario_yuyuko_", "data/scenario/yuyuko/" },
		{ "data_scenario_yukari_effect_", "data/scenario/yukari/effect/" },
		{ "data_scenario_yukari_", "data/scenario/yukari/" },
		{ "data_scenario_youmu_effect_", "data/scenario/youmu/effect/" },
		{ "data_scenario_youmu_", "data/scenario/youmu/" },
		{ "data_scenario_utsuho_", "data/scenario/utsuho/" },
		{ "data_scenario_udonge_effect_", "data/scenario/udonge/effect/" },
		{ "data_scenario_udonge_", "data/scenario/udonge/" },
		{ "data_scenario_tenshi_effect_", "data/scenario/tenshi/effect/" },
		{ "data_scenario_tenshi_", "data/scenario/tenshi/" },
		{ "data_scenario_suwako_", "data/scenario/suwako/" },
		{ "data_scenario_suika_effect_", "data/scenario/suika/effect/" },
		{ "data_scenario_suika_", "data/scenario/suika/" },
		{ "data_scenario_sanae_effect_", "data/scenario/sanae/effect/" },
		{ "data_scenario_sanae_", "data/scenario/sanae/" },
		{ "data_scenario_sakuya_effect_", "data/scenario/sakuya/effect/" },
		{ "data_scenario_sakuya_", "data/scenario/sakuya/" },
		{ "data_scenario_remilia_effect_", "data/scenario/remilia/effect/" },
		{ "data_scenario_remilia_", "data/scenario/remilia/" },
		{ "data_scenario_reimu_effect_", "data/scenario/reimu/effect/" },
		{ "data_scenario_reimu_", "data/scenario/reimu/" },
		{ "data_scenario_patchouli_effect_", "data/scenario/patchouli/effect/" },
		{ "data_scenario_patchouli_", "data/scenario/patchouli/" },
		{ "data_scenario_meirin_effect_", "data/scenario/meirin/effect/" },
		{ "data_scenario_meirin_", "data/scenario/meirin/" },
		{ "data_scenario_marisa_effect_", "data/scenario/marisa/effect/" },
		{ "data_scenario_marisa_", "data/scenario/marisa/" },
		{ "data_scenario_komachi_effect_", "data/scenario/komachi/effect/" },
		{ "data_scenario_komachi_", "data/scenario/komachi/" },
		{ "data_scenario_iku_effect_", "data/scenario/iku/effect/" },
		{ "data_scenario_iku_", "data/scenario/iku/" },
		{ "data_scenario_effect_", "data/scenario/effect/" },
		{ "data_scenario_chirno_effect_", "data/scenario/chirno/effect/" },
		{ "data_scenario_chirno_", "data/scenario/chirno/" },
		{ "data_scenario_aya_effect_", "data/scenario/aya/effect/" },
		{ "data_scenario_aya_", "data/scenario/aya/" },
		{ "data_scenario_alice_effect_", "data/scenario/alice/effect/" },
		{ "data_scenario_alice_", "data/scenario/alice/" },
		{ "data_scenario_", "data/scenario/" },
		{ "data_profile_keyconfig_", "data/profile/keyconfig/" },
		{ "data_profile_deck2_name_", "data/profile/deck2/name/" },
		{ "data_profile_deck2_", "data/profile/deck2/" },
		{ "data_profile_deck1_", "data/profile/deck1/" },
		{ "data_profile_", "data/profile/" },
		{ "data_number_", "data/number/" },
		{ "data_menu_select_", "data/menu/select/" },
		{ "data_menu_result_", "data/menu/result/" },
		{ "data_menu_replay_", "data/menu/replay/" },
		{ "data_menu_practice_", "data/menu/practice/" },
		{ "data_menu_musicroom_", "data/menu/musicroom/" },
		{ "data_menu_gear_", "data/menu/gear/" },
		{ "data_menu_continue_", "data/menu/continue/" },
		{ "data_menu_connect_", "data/menu/connect/" },
		{ "data_menu_config_", "data/menu/config/" },
		{ "data_menu_battle_", "data/menu/battle/" },
		{ "data_menu_", "data/menu/" },
		{ "data_infoeffect_result_", "data/infoeffect/result/" },
		{ "data_infoeffect_", "data/infoeffect/" },
		{ "data_guide_", "data/guide/" },
		{ "data_effect_", "data/effect/" },
		{ "data_csv_yuyuko_", "data/csv/yuyuko/" },
		{ "data_csv_yukari_", "data/csv/yukari/" },
		{ "data_csv_youmu_", "data/csv/youmu/" },
		{ "data_csv_utsuho_", "data/csv/utsuho/" },
		{ "data_csv_udonge_", "data/csv/udonge/" },
		{ "data_csv_tenshi_", "data/csv/tenshi/" },
		{ "data_csv_system_", "data/csv/system/" },
		{ "data_csv_suwako_", "data/csv/suwako/" },
		{ "data_csv_suika_", "data/csv/suika/" },
		{ "data_csv_sanae_", "data/csv/sanae/" },
		{ "data_csv_sakuya_", "data/csv/sakuya/" },
		{ "data_csv_remilia_", "data/csv/remilia/" },
		{ "data_csv_reimu_", "data/csv/reimu/" },
		{ "data_csv_patchouli_", "data/csv/patchouli/" },
		{ "data_csv_namazu_", "data/csv/namazu/" },
		{ "data_csv_meirin_", "data/csv/meirin/" },
		{ "data_csv_marisa_", "data/csv/marisa/" },
		{ "data_csv_komachi_", "data/csv/komachi/" },
		{ "data_csv_iku_", "data/csv/iku/" },
		{ "data_csv_common_", "data/csv/common/" },
		{ "data_csv_chirno_", "data/csv/chirno/" },
		{ "data_csv_background_", "data/csv/background/" },
		{ "data_csv_aya_", "data/csv/aya/" },
		{ "data_csv_alice_", "data/csv/alice/" },
		{ "data_csv_", "data/csv/" },
		{ "data_character_yuyuko_stand_", "data/character/yuyuko/stand/" },
		{ "data_character_yuyuko_face_", "data/character/yuyuko/face/" },
		{ "data_character_yuyuko_back_", "data/character/yuyuko/back/" },
		{ "data_character_yuyuko_", "data/character/yuyuko/" },
		{ "data_character_yukari_stand_", "data/character/yukari/stand/" },
		{ "data_character_yukari_face_", "data/character/yukari/face/" },
		{ "data_character_yukari_back_", "data/character/yukari/back/" },
		{ "data_character_yukari_", "data/character/yukari/" },
		{ "data_character_youmu_stand_", "data/character/youmu/stand/" },
		{ "data_character_youmu_face_", "data/character/youmu/face/" },
		{ "data_character_youmu_back_", "data/character/youmu/back/" },
		{ "data_character_youmu_", "data/character/youmu/" },
		{ "data_character_utsuho_stand_", "data/character/utsuho/stand/" },
		{ "data_character_utsuho_face_", "data/character/utsuho/face/" },
		{ "data_character_utsuho_back_", "data/character/utsuho/back/" },
		{ "data_character_utsuho_", "data/character/utsuho/" },
		{ "data_character_udonge_stand_", "data/character/udonge/stand/" },
		{ "data_character_udonge_face_", "data/character/udonge/face/" },
		{ "data_character_udonge_back_", "data/character/udonge/back/" },
		{ "data_character_udonge_", "data/character/udonge/" },
		{ "data_character_tenshi_stand_", "data/character/tenshi/stand/" },
		{ "data_character_tenshi_face_", "data/character/tenshi/face/" },
		{ "data_character_tenshi_back_", "data/character/tenshi/back/" },
		{ "data_character_tenshi_", "data/character/tenshi/" },
		{ "data_character_suwako_stand_", "data/character/suwako/stand/" },
		{ "data_character_suwako_face_", "data/character/suwako/face/" },
		{ "data_character_suwako_back_", "data/character/suwako/back/" },
		{ "data_character_suwako_", "data/character/suwako/" },
		{ "data_character_suika_stand_", "data/character/suika/stand/" },
		{ "data_character_suika_face_", "data/character/suika/face/" },
		{ "data_character_suika_back_", "data/character/suika/back/" },
		{ "data_character_suika_", "data/character/suika/" },
		{ "data_character_sanae_stand_", "data/character/sanae/stand/" },
		{ "data_character_sanae_face_", "data/character/sanae/face/" },
		{ "data_character_sanae_back_", "data/character/sanae/back/" },
		{ "data_character_sanae_", "data/character/sanae/" },
		{ "data_character_sakuya_stand_", "data/character/sakuya/stand/" },
		{ "data_character_sakuya_face_", "data/character/sakuya/face/" },
		{ "data_character_sakuya_back_", "data/character/sakuya/back/" },
		{ "data_character_sakuya_", "data/character/sakuya/" },
		{ "data_character_remilia_stand_", "data/character/remilia/stand/" },
		{ "data_character_remilia_face_", "data/character/remilia/face/" },
		{ "data_character_remilia_back_", "data/character/remilia/back/" },
		{ "data_character_remilia_", "data/character/remilia/" },
		{ "data_character_reimu_stand_", "data/character/reimu/stand/" },
		{ "data_character_reimu_face_", "data/character/reimu/face/" },
		{ "data_character_reimu_back_", "data/character/reimu/back/" },
		{ "data_character_reimu_", "data/character/reimu/" },
		{ "data_character_patchouli_stand_", "data/character/patchouli/stand/" },
		{ "data_character_patchouli_face_", "data/character/patchouli/face/" },
		{ "data_character_patchouli_back_", "data/character/patchouli/back/" },
		{ "data_character_patchouli_", "data/character/patchouli/" },
		{ "data_character_namazu_face_", "data/character/namazu/face/" },
		{ "data_character_namazu_back_", "data/character/namazu/back/" },
		{ "data_character_namazu_", "data/character/namazu/" },
		{ "data_character_meirin_stand_", "data/character/meirin/stand/" },
		{ "data_character_meirin_face_", "data/character/meirin/face/" },
		{ "data_character_meirin_back_", "data/character/meirin/back/" },
		{ "data_character_meirin_", "data/character/meirin/" },
		{ "data_character_marisa_stand_", "data/character/marisa/stand/" },
		{ "data_character_marisa_face_", "data/character/marisa/face/" },
		{ "data_character_marisa_back_", "data/character/marisa/back/" },
		{ "data_character_marisa_", "data/character/marisa/" },
		{ "data_character_komachi_stand_", "data/character/komachi/stand/" },
		{ "data_character_komachi_face_", "data/character/komachi/face/" },
		{ "data_character_komachi_back_", "data/character/komachi/back/" },
		{ "data_character_komachi_", "data/character/komachi/" },
		{ "data_character_iku_stand_", "data/character/iku/stand/" },
		{ "data_character_iku_face_", "data/character/iku/face/" },
		{ "data_character_iku_back_", "data/character/iku/back/" },
		{ "data_character_iku_", "data/character/iku/" },
		{ "data_character_common_", "data/character/common/" },
		{ "data_character_chirno_stand_", "data/character/chirno/stand/" },
		{ "data_character_chirno_face_", "data/character/chirno/face/" },
		{ "data_character_chirno_back_", "data/character/chirno/back/" },
		{ "data_character_chirno_", "data/character/chirno/" },
		{ "data_character_aya_stand_", "data/character/aya/stand/" },
		{ "data_character_aya_face_", "data/character/aya/face/" },
		{ "data_character_aya_back_", "data/character/aya/back/" },
		{ "data_character_aya_", "data/character/aya/" },
		{ "data_character_alice_stand_", "data/character/alice/stand/" },
		{ "data_character_alice_face_", "data/character/alice/face/" },
		{ "data_character_alice_back_", "data/character/alice/back/" },
		{ "data_character_alice_", "data/character/alice/" },
		{ "data_character_", "data/character/" },
		{ "data_card_yuyuko_", "data/card/yuyuko/" },
		{ "data_card_yukari_", "data/card/yukari/" },
		{ "data_card_youmu_", "data/card/youmu/" },
		{ "data_card_utsuho_", "data/card/utsuho/" },
		{ "data_card_udonge_", "data/card/udonge/" },
		{ "data_card_tenshi_", "data/card/tenshi/" },
		{ "data_card_suwako_", "data/card/suwako/" },
		{ "data_card_suika_", "data/card/suika/" },
		{ "data_card_sanae_", "data/card/sanae/" },
		{ "data_card_sakuya_", "data/card/sakuya/" },
		{ "data_card_remilia_", "data/card/remilia/" },
		{ "data_card_reimu_", "data/card/reimu/" },
		{ "data_card_patchouli_", "data/card/patchouli/" },
		{ "data_card_meirin_", "data/card/meirin/" },
		{ "data_card_marisa_", "data/card/marisa/" },
		{ "data_card_komachi_", "data/card/komachi/" },
		{ "data_card_iku_", "data/card/iku/" },
		{ "data_card_common_", "data/card/common/" },
		{ "data_card_chirno_", "data/card/chirno/" },
		{ "data_card_aya_", "data/card/aya/" },
		{ "data_card_alice_", "data/card/alice/" },
		{ "data_card_", "data/card/" },
		{ "data_bgm_", "data/bgm/" },
		{ "data_battle_", "data/battle/" },
		{ "data_background_bg38_", "data/background/bg38/" },
		{ "data_background_bg37_", "data/background/bg37/" },
		{ "data_background_bg36_", "data/background/bg36/" },
		{ "data_background_bg34_object_", "data/background/bg34/object/" },
		{ "data_background_bg34_", "data/background/bg34/" },
		{ "data_background_bg33_", "data/background/bg33/" },
		{ "data_background_bg32_", "data/background/bg32/" },
		{ "data_background_bg31_", "data/background/bg31/" },
		{ "data_background_bg30_", "data/background/bg30/" },
		{ "data_background_bg18_", "data/background/bg18/" },
		{ "data_background_bg17_", "data/background/bg17/" },
		{ "data_background_bg16_", "data/background/bg16/" },
		{ "data_background_bg15_", "data/background/bg15/" },
		{ "data_background_bg14_", "data/background/bg14/" },
		{ "data_background_bg13_", "data/background/bg13/" },
		{ "data_background_bg12_", "data/background/bg12/" },
		{ "data_background_bg11_", "data/background/bg11/" },
		{ "data_background_bg10_", "data/background/bg10/" },
		{ "data_background_bg06_", "data/background/bg06/" },
		{ "data_background_bg05_", "data/background/bg05/" },
		{ "data_background_bg04_", "data/background/bg04/" },
		{ "data_background_bg03_", "data/background/bg03/" },
		{ "data_background_bg02_right_", "data/background/bg02/right/" },
		{ "data_background_bg02_left_", "data/background/bg02/left/" },
		{ "data_background_bg02_center_", "data/background/bg02/center/" },
		{ "data_background_bg02_", "data/background/bg02/" },
		{ "data_background_bg01_", "data/background/bg01/" },
		{ "data_background_bg00_", "data/background/bg00/" },
		{ "data_background_", "data/background/" },
		{ "data_", "data/" },
	}; // TODO Tree search

	const char* oldName = entry.getName();
	char newName[256];

	for (auto dir : dirTable) {
		size_t len = strlen(dir[0]);
		if (strncmp(oldName, dir[0], len) == 0) {
			memcpy(newName, dir[1], len);
			strcpy(newName + len, oldName + len);
			return filter.renameEntry(newName);
		}
	}

	return false;
}

void ShadyCore::PackageFilter::apply(Package& package, Filter filterType, Callback* callback) {
	auto filterFunc =
		filterType == FILTER_FROM_ZIP_TEXT_EXTENSION ? FilterFromZipTextFileExtension :
		filterType == FILTER_TO_ZIP_TEXT_EXTENSION ? FilterToZipTextFileExtension :
		filterType == FILTER_TO_ZIP_CONVERTER ? FilterToZipConvert :
		filterType == FILTER_DECRYPT_ALL ? FilterDecryptAll :
		filterType == FILTER_ENCRYPT_ALL ? FilterEncryptAll :
		filterType == FILTER_SLASH_TO_UNDERLINE ? FilterSlashToUnderline :
		filterType == FILTER_UNDERLINE_TO_SLASH ? FilterUnderlineToSlash :
		0;
	PackageFilter filter(package);
	filter.iter = package.begin();
	unsigned int i = 1;
	while (filter.iter != package.end()) {
		if (callback) callback(filter.iter->getName(), i, package.size());
		if (!filterFunc(filter, *filter.iter)) ++filter.iter;
		else ++i;
	}
}

bool ShadyCore::PackageFilter::renameEntry(const char* name) {
	iter = package.renameFile(iter, name);
	return true;
}

bool ShadyCore::PackageFilter::convertEntry(const FileType& type) {
	boost::filesystem::path tempFile = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
	std::ofstream output(tempFile.string().c_str(), std::ios::binary);
	ShadyCore::convertResource(type, iter->open(), output);
	output.close(); iter->close();

	boost::filesystem::path path(iter->getName());
	path.replace_extension(type.inverseExt);
	package.appendFile(path.generic_string().c_str(), tempFile.string().c_str(), true);

	iter = package.detachFile(iter);
	return true;
}

bool ShadyCore::PackageFilter::convertData(const FileType& type) {
	boost::filesystem::path tempFile = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
	std::ofstream output(tempFile.string().c_str(), std::ios::binary);
	ShadyCore::convertResource(type, iter->open(), output);
	output.close(); iter->close();

	package.appendFile(iter->getName(), tempFile.string().c_str(), true);
	return false;
}
