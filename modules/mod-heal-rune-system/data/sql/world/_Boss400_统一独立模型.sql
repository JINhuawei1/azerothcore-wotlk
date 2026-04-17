-- Boss400 统一独立模型
-- 最终真源：统一管理 395001-395300 与 396001-396100 的最终独立模型，避免基础 SQL 覆盖

-- ===== 世界Boss395 / 神符Boss / 切割Boss 最终独立模型 =====
-- 回血Boss 300独立模型更新 (395001-395100)
-- 每个Boss一个独特模型，不重复
UPDATE `creature_template_model` SET `DisplayScale` = 3.0
 WHERE `CreatureID` BETWEEN 395001 AND 395200 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 10709 WHERE `CreatureID` = 395001 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 10701 WHERE `CreatureID` = 395002 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 10626 WHERE `CreatureID` = 395003 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 10452 WHERE `CreatureID` = 395004 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 10443 WHERE `CreatureID` = 395005 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 10432 WHERE `CreatureID` = 395006 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 10374 WHERE `CreatureID` = 395007 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 10356 WHERE `CreatureID` = 395008 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 10355 WHERE `CreatureID` = 395009 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 10354 WHERE `CreatureID` = 395010 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 10346 WHERE `CreatureID` = 395011 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 10054 WHERE `CreatureID` = 395012 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 9750 WHERE `CreatureID` = 395013 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 9591 WHERE `CreatureID` = 395014 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 9562 WHERE `CreatureID` = 395015 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 9534 WHERE `CreatureID` = 395016 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 9531 WHERE `CreatureID` = 395017 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 9530 WHERE `CreatureID` = 395018 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 9529 WHERE `CreatureID` = 395019 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 9491 WHERE `CreatureID` = 395020 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 9448 WHERE `CreatureID` = 395021 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 9444 WHERE `CreatureID` = 395022 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 9418 WHERE `CreatureID` = 395023 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 9372 WHERE `CreatureID` = 395024 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 9135 WHERE `CreatureID` = 395025 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 9013 WHERE `CreatureID` = 395026 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 8900 WHERE `CreatureID` = 395027 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 8870 WHERE `CreatureID` = 395028 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 8834 WHERE `CreatureID` = 395029 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 8574 WHERE `CreatureID` = 395030 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 8550 WHERE `CreatureID` = 395031 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 8471 WHERE `CreatureID` = 395032 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 8390 WHERE `CreatureID` = 395033 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 8389 WHERE `CreatureID` = 395034 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 8129 WHERE `CreatureID` = 395035 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 7975 WHERE `CreatureID` = 395036 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 7892 WHERE `CreatureID` = 395037 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 7856 WHERE `CreatureID` = 395038 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 7848 WHERE `CreatureID` = 395039 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 7847 WHERE `CreatureID` = 395040 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 7844 WHERE `CreatureID` = 395041 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 7840 WHERE `CreatureID` = 395042 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 7819 WHERE `CreatureID` = 395043 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 7807 WHERE `CreatureID` = 395044 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 7569 WHERE `CreatureID` = 395045 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 7509 WHERE `CreatureID` = 395046 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 7349 WHERE `CreatureID` = 395047 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 7336 WHERE `CreatureID` = 395048 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 7232 WHERE `CreatureID` = 395049 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 7049 WHERE `CreatureID` = 395050 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 7043 WHERE `CreatureID` = 395051 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 6889 WHERE `CreatureID` = 395052 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 6818 WHERE `CreatureID` = 395053 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 6800 WHERE `CreatureID` = 395054 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 6763 WHERE `CreatureID` = 395055 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 6743 WHERE `CreatureID` = 395056 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 6695 WHERE `CreatureID` = 395057 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 6693 WHERE `CreatureID` = 395058 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 6692 WHERE `CreatureID` = 395059 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 6380 WHERE `CreatureID` = 395060 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 6212 WHERE `CreatureID` = 395061 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 6116 WHERE `CreatureID` = 395062 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 6113 WHERE `CreatureID` = 395063 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 6085 WHERE `CreatureID` = 395064 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 6082 WHERE `CreatureID` = 395065 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 6068 WHERE `CreatureID` = 395066 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 6041 WHERE `CreatureID` = 395067 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 5927 WHERE `CreatureID` = 395068 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 5782 WHERE `CreatureID` = 395069 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 5781 WHERE `CreatureID` = 395070 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 5774 WHERE `CreatureID` = 395071 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 5773 WHERE `CreatureID` = 395072 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 5772 WHERE `CreatureID` = 395073 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 5747 WHERE `CreatureID` = 395074 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 5561 WHERE `CreatureID` = 395075 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 5430 WHERE `CreatureID` = 395076 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 5286 WHERE `CreatureID` = 395077 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 5243 WHERE `CreatureID` = 395078 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 5229 WHERE `CreatureID` = 395079 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 5047 WHERE `CreatureID` = 395080 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 5026 WHERE `CreatureID` = 395081 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 4982 WHERE `CreatureID` = 395082 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 4979 WHERE `CreatureID` = 395083 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 4978 WHERE `CreatureID` = 395084 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 4973 WHERE `CreatureID` = 395085 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 4943 WHERE `CreatureID` = 395086 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 4937 WHERE `CreatureID` = 395087 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 4920 WHERE `CreatureID` = 395088 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 4914 WHERE `CreatureID` = 395089 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 4913 WHERE `CreatureID` = 395090 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 4912 WHERE `CreatureID` = 395091 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 4910 WHERE `CreatureID` = 395092 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 4874 WHERE `CreatureID` = 395093 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 4762 WHERE `CreatureID` = 395094 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 4629 WHERE `CreatureID` = 395095 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 4597 WHERE `CreatureID` = 395096 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 4596 WHERE `CreatureID` = 395097 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 4595 WHERE `CreatureID` = 395098 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 4593 WHERE `CreatureID` = 395099 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 4592 WHERE `CreatureID` = 395100 AND `Idx` = 0;
-- 切割Boss 300独立模型更新 (395101-395200)
-- 每个Boss一个独特模型，不重复
UPDATE `creature_template_model` SET `CreatureDisplayID` = 4585 WHERE `CreatureID` = 395101 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 4458 WHERE `CreatureID` = 395102 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 4212 WHERE `CreatureID` = 395103 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 4156 WHERE `CreatureID` = 395104 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 4026 WHERE `CreatureID` = 395105 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 3898 WHERE `CreatureID` = 395106 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 3616 WHERE `CreatureID` = 395107 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 3589 WHERE `CreatureID` = 395108 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 3535 WHERE `CreatureID` = 395109 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 3341 WHERE `CreatureID` = 395110 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 3320 WHERE `CreatureID` = 395111 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 3267 WHERE `CreatureID` = 395112 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 3212 WHERE `CreatureID` = 395113 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 3208 WHERE `CreatureID` = 395114 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 3186 WHERE `CreatureID` = 395115 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 3030 WHERE `CreatureID` = 395116 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 2879 WHERE `CreatureID` = 395117 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 2850 WHERE `CreatureID` = 395118 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 2714 WHERE `CreatureID` = 395119 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 2713 WHERE `CreatureID` = 395120 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 2703 WHERE `CreatureID` = 395121 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 2702 WHERE `CreatureID` = 395122 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 2687 WHERE `CreatureID` = 395123 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 2597 WHERE `CreatureID` = 395124 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 2582 WHERE `CreatureID` = 395125 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 2549 WHERE `CreatureID` = 395126 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 2541 WHERE `CreatureID` = 395127 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 2537 WHERE `CreatureID` = 395128 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 2491 WHERE `CreatureID` = 395129 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 2355 WHERE `CreatureID` = 395130 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 2346 WHERE `CreatureID` = 395131 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 2296 WHERE `CreatureID` = 395132 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 2174 WHERE `CreatureID` = 395133 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 2168 WHERE `CreatureID` = 395134 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 2076 WHERE `CreatureID` = 395135 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 1994 WHERE `CreatureID` = 395136 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 1973 WHERE `CreatureID` = 395137 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 1961 WHERE `CreatureID` = 395138 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 1933 WHERE `CreatureID` = 395139 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 1921 WHERE `CreatureID` = 395140 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 1912 WHERE `CreatureID` = 395141 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 1817 WHERE `CreatureID` = 395142 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 1549 WHERE `CreatureID` = 395143 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 1534 WHERE `CreatureID` = 395144 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 1306 WHERE `CreatureID` = 395145 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 1305 WHERE `CreatureID` = 395146 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 1204 WHERE `CreatureID` = 395147 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 1194 WHERE `CreatureID` = 395148 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 1162 WHERE `CreatureID` = 395149 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 1104 WHERE `CreatureID` = 395150 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 1103 WHERE `CreatureID` = 395151 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 1092 WHERE `CreatureID` = 395152 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 1091 WHERE `CreatureID` = 395153 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 1078 WHERE `CreatureID` = 395154 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 1065 WHERE `CreatureID` = 395155 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 1043 WHERE `CreatureID` = 395156 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 1019 WHERE `CreatureID` = 395157 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 1018 WHERE `CreatureID` = 395158 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 1012 WHERE `CreatureID` = 395159 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 1011 WHERE `CreatureID` = 395160 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 985 WHERE `CreatureID` = 395161 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 982 WHERE `CreatureID` = 395162 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 965 WHERE `CreatureID` = 395163 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 963 WHERE `CreatureID` = 395164 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 955 WHERE `CreatureID` = 395165 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 931 WHERE `CreatureID` = 395166 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 913 WHERE `CreatureID` = 395167 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 904 WHERE `CreatureID` = 395168 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 831 WHERE `CreatureID` = 395169 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 830 WHERE `CreatureID` = 395170 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 821 WHERE `CreatureID` = 395171 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 788 WHERE `CreatureID` = 395172 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 780 WHERE `CreatureID` = 395173 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 774 WHERE `CreatureID` = 395174 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 720 WHERE `CreatureID` = 395175 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 706 WHERE `CreatureID` = 395176 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 682 WHERE `CreatureID` = 395177 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 652 WHERE `CreatureID` = 395178 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 631 WHERE `CreatureID` = 395179 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 625 WHERE `CreatureID` = 395180 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 610 WHERE `CreatureID` = 395181 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 548 WHERE `CreatureID` = 395182 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 543 WHERE `CreatureID` = 395183 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 540 WHERE `CreatureID` = 395184 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 536 WHERE `CreatureID` = 395185 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 525 WHERE `CreatureID` = 395186 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 522 WHERE `CreatureID` = 395187 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 519 WHERE `CreatureID` = 395188 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 511 WHERE `CreatureID` = 395189 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 507 WHERE `CreatureID` = 395190 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 500 WHERE `CreatureID` = 395191 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 497 WHERE `CreatureID` = 395192 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 491 WHERE `CreatureID` = 395193 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 441 WHERE `CreatureID` = 395194 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 418 WHERE `CreatureID` = 395195 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 391 WHERE `CreatureID` = 395196 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 388 WHERE `CreatureID` = 395197 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 383 WHERE `CreatureID` = 395198 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 368 WHERE `CreatureID` = 395199 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 360 WHERE `CreatureID` = 395200 AND `Idx` = 0;

-- ===== 魔次Boss 最终独立模型 =====
UPDATE `creature_template_model`
SET `DisplayScale` = 3.0
WHERE `CreatureID` BETWEEN 395201 AND 395300 AND `Idx` = 0;

UPDATE `creature_template_model`
SET `CreatureDisplayID` = CASE
    WHEN CreatureID = 395201 THEN 27006
    WHEN CreatureID = 395202 THEN 26285
    WHEN CreatureID = 395203 THEN 275
    WHEN CreatureID = 395204 THEN 152
    WHEN CreatureID = 395205 THEN 24564
    WHEN CreatureID = 395206 THEN 16174
    WHEN CreatureID = 395207 THEN 29268
    WHEN CreatureID = 395208 THEN 28019
    WHEN CreatureID = 395209 THEN 24191
    WHEN CreatureID = 395210 THEN 21601
    WHEN CreatureID = 395211 THEN 8570
    WHEN CreatureID = 395212 THEN 31005
    WHEN CreatureID = 395213 THEN 29614
    WHEN CreatureID = 395214 THEN 29267
    WHEN CreatureID = 395215 THEN 29240
    WHEN CreatureID = 395216 THEN 18698
    WHEN CreatureID = 395217 THEN 31761
    WHEN CreatureID = 395218 THEN 31165
    WHEN CreatureID = 395219 THEN 31119
    WHEN CreatureID = 395220 THEN 30893
    WHEN CreatureID = 395221 THEN 30881
    WHEN CreatureID = 395222 THEN 30858
    WHEN CreatureID = 395223 THEN 30857
    WHEN CreatureID = 395224 THEN 30856
    WHEN CreatureID = 395225 THEN 30790
    WHEN CreatureID = 395226 THEN 29815
    WHEN CreatureID = 395227 THEN 29615
    WHEN CreatureID = 395228 THEN 28977
    WHEN CreatureID = 395229 THEN 28817
    WHEN CreatureID = 395230 THEN 28638
    WHEN CreatureID = 395231 THEN 27153
    WHEN CreatureID = 395232 THEN 26752
    WHEN CreatureID = 395233 THEN 21793
    WHEN CreatureID = 395234 THEN 19274
    WHEN CreatureID = 395235 THEN 15945
    WHEN CreatureID = 395236 THEN 15787
    WHEN CreatureID = 395237 THEN 30865
    WHEN CreatureID = 395238 THEN 30318
    WHEN CreatureID = 395239 THEN 28611
    WHEN CreatureID = 395240 THEN 25337
    WHEN CreatureID = 395241 THEN 22256
    WHEN CreatureID = 395242 THEN 22209
    WHEN CreatureID = 395243 THEN 21899
    WHEN CreatureID = 395244 THEN 21831
    WHEN CreatureID = 395245 THEN 21830
    WHEN CreatureID = 395246 THEN 18527
    WHEN CreatureID = 395247 THEN 16590
    WHEN CreatureID = 395248 THEN 16309
    WHEN CreatureID = 395249 THEN 16033
    WHEN CreatureID = 395250 THEN 15432
    WHEN CreatureID = 395251 THEN 11380
    WHEN CreatureID = 395252 THEN 32179
    WHEN CreatureID = 395253 THEN 31577
    WHEN CreatureID = 395254 THEN 31089
    WHEN CreatureID = 395255 THEN 29524
    WHEN CreatureID = 395256 THEN 29185
    WHEN CreatureID = 395257 THEN 29176
    WHEN CreatureID = 395258 THEN 29175
    WHEN CreatureID = 395259 THEN 29174
    WHEN CreatureID = 395260 THEN 29082
    WHEN CreatureID = 395261 THEN 29041
    WHEN CreatureID = 395262 THEN 28875
    WHEN CreatureID = 395263 THEN 28831
    WHEN CreatureID = 395264 THEN 28787
    WHEN CreatureID = 395265 THEN 28777
    WHEN CreatureID = 395266 THEN 28743
    WHEN CreatureID = 395267 THEN 28651
    WHEN CreatureID = 395268 THEN 28641
    WHEN CreatureID = 395269 THEN 28548
    WHEN CreatureID = 395270 THEN 28488
    WHEN CreatureID = 395271 THEN 28381
    WHEN CreatureID = 395272 THEN 28344
    WHEN CreatureID = 395273 THEN 28324
    WHEN CreatureID = 395274 THEN 27421
    WHEN CreatureID = 395275 THEN 27108
    WHEN CreatureID = 395276 THEN 27082
    WHEN CreatureID = 395277 THEN 27039
    WHEN CreatureID = 395278 THEN 27035
    WHEN CreatureID = 395279 THEN 26967
    WHEN CreatureID = 395280 THEN 26935
    WHEN CreatureID = 395281 THEN 25501
    WHEN CreatureID = 395282 THEN 24213
    WHEN CreatureID = 395283 THEN 16582
    WHEN CreatureID = 395284 THEN 16279
    WHEN CreatureID = 395285 THEN 16155
    WHEN CreatureID = 395286 THEN 16154
    WHEN CreatureID = 395287 THEN 16153
    WHEN CreatureID = 395288 THEN 16137
    WHEN CreatureID = 395289 THEN 16110
    WHEN CreatureID = 395290 THEN 16064
    WHEN CreatureID = 395291 THEN 16035
    WHEN CreatureID = 395292 THEN 15940
    WHEN CreatureID = 395293 THEN 15931
    WHEN CreatureID = 395294 THEN 15928
    WHEN CreatureID = 395295 THEN 15295
    WHEN CreatureID = 395296 THEN 31093
    WHEN CreatureID = 395297 THEN 30993
    WHEN CreatureID = 395298 THEN 29860
    WHEN CreatureID = 395299 THEN 29816
    WHEN CreatureID = 395300 THEN 29073
    ELSE `CreatureDisplayID`
END
WHERE `CreatureID` BETWEEN 395201 AND 395300 AND `Idx` = 0;


-- ===== 称号Boss 最终独立模型 =====
-- 称号Boss 300独立模型更新 (396001-396100)
-- 每个Boss一个独特模型，不重复
UPDATE `creature_template_model` SET `DisplayScale` = 3.0
 WHERE `CreatureID` BETWEEN 396001 AND 396100 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 26193 WHERE `CreatureID` = 396001 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 26087 WHERE `CreatureID` = 396002 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 25656 WHERE `CreatureID` = 396003 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 24106 WHERE `CreatureID` = 396004 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 23685 WHERE `CreatureID` = 396005 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 23136 WHERE `CreatureID` = 396006 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 16214 WHERE `CreatureID` = 396007 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11402 WHERE `CreatureID` = 396008 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 1206 WHERE `CreatureID` = 396009 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 28818 WHERE `CreatureID` = 396010 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 28248 WHERE `CreatureID` = 396011 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 28239 WHERE `CreatureID` = 396012 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 28230 WHERE `CreatureID` = 396013 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 27504 WHERE `CreatureID` = 396014 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 26286 WHERE `CreatureID` = 396015 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 25680 WHERE `CreatureID` = 396016 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 21180 WHERE `CreatureID` = 396017 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 20862 WHERE `CreatureID` = 396018 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 20810 WHERE `CreatureID` = 396019 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 20771 WHERE `CreatureID` = 396020 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 20770 WHERE `CreatureID` = 396021 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 20769 WHERE `CreatureID` = 396022 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 20768 WHERE `CreatureID` = 396023 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 20767 WHERE `CreatureID` = 396024 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 20766 WHERE `CreatureID` = 396025 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 20765 WHERE `CreatureID` = 396026 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 20764 WHERE `CreatureID` = 396027 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 20763 WHERE `CreatureID` = 396028 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 20762 WHERE `CreatureID` = 396029 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 20761 WHERE `CreatureID` = 396030 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 20590 WHERE `CreatureID` = 396031 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 20044 WHERE `CreatureID` = 396032 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 19824 WHERE `CreatureID` = 396033 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 19816 WHERE `CreatureID` = 396034 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 19681 WHERE `CreatureID` = 396035 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 18070 WHERE `CreatureID` = 396036 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 17625 WHERE `CreatureID` = 396037 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 17445 WHERE `CreatureID` = 396038 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 16406 WHERE `CreatureID` = 396039 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 16176 WHERE `CreatureID` = 396040 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 16170 WHERE `CreatureID` = 396041 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 16167 WHERE `CreatureID` = 396042 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 14528 WHERE `CreatureID` = 396043 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 14526 WHERE `CreatureID` = 396044 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 14525 WHERE `CreatureID` = 396045 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 14523 WHERE `CreatureID` = 396046 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 14497 WHERE `CreatureID` = 396047 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 14315 WHERE `CreatureID` = 396048 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 14313 WHERE `CreatureID` = 396049 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 14272 WHERE `CreatureID` = 396050 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 14257 WHERE `CreatureID` = 396051 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 14255 WHERE `CreatureID` = 396052 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 12819 WHERE `CreatureID` = 396053 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 12342 WHERE `CreatureID` = 396054 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 12336 WHERE `CreatureID` = 396055 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 12073 WHERE `CreatureID` = 396056 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11640 WHERE `CreatureID` = 396057 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11570 WHERE `CreatureID` = 396058 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11566 WHERE `CreatureID` = 396059 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11564 WHERE `CreatureID` = 396060 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11562 WHERE `CreatureID` = 396061 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11532 WHERE `CreatureID` = 396062 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11511 WHERE `CreatureID` = 396063 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11510 WHERE `CreatureID` = 396064 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11453 WHERE `CreatureID` = 396065 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11422 WHERE `CreatureID` = 396066 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11414 WHERE `CreatureID` = 396067 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11413 WHERE `CreatureID` = 396068 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11412 WHERE `CreatureID` = 396069 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11347 WHERE `CreatureID` = 396070 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11331 WHERE `CreatureID` = 396071 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11319 WHERE `CreatureID` = 396072 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11316 WHERE `CreatureID` = 396073 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11293 WHERE `CreatureID` = 396074 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11262 WHERE `CreatureID` = 396075 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11261 WHERE `CreatureID` = 396076 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11257 WHERE `CreatureID` = 396077 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11181 WHERE `CreatureID` = 396078 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11179 WHERE `CreatureID` = 396079 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11142 WHERE `CreatureID` = 396080 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11140 WHERE `CreatureID` = 396081 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11106 WHERE `CreatureID` = 396082 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11099 WHERE `CreatureID` = 396083 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11096 WHERE `CreatureID` = 396084 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11092 WHERE `CreatureID` = 396085 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11084 WHERE `CreatureID` = 396086 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11012 WHERE `CreatureID` = 396087 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 10983 WHERE `CreatureID` = 396088 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 10921 WHERE `CreatureID` = 396089 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 10920 WHERE `CreatureID` = 396090 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 10911 WHERE `CreatureID` = 396091 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 10904 WHERE `CreatureID` = 396092 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 10889 WHERE `CreatureID` = 396093 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 10850 WHERE `CreatureID` = 396094 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 10819 WHERE `CreatureID` = 396095 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 10807 WHERE `CreatureID` = 396096 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 10802 WHERE `CreatureID` = 396097 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 10800 WHERE `CreatureID` = 396098 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 10792 WHERE `CreatureID` = 396099 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 10771 WHERE `CreatureID` = 396100 AND `Idx` = 0;
