
// - insert whitespace between keywords, e.g. `DVDVideo` -> `DVD Video`
// - replace 'programmer whitespace' `_` with actual whitespace.
// - assuming the input is a filename acting as (movie, music, etc.) media title: detect trailing year info in parens, e.g. ` (1995)` and technology keywords, e.g. `H265`, `BluRay`, `4K`, `HDR`, `DVD`, `WEBRip`, `WEB-DL`, `HDTV` and move them to the tail end of the output string, separated by commas and surrounded by brackets: these all serve as a kind of tags.
// - ditto for `.` dots acting as whitespace: replace with actual ` ` space characters.
// - ditto for `-` dashes acting as whitespace: replace with actual ` ` space characters.
// - collapse multiple subsequent whitespace characters into a single ` ` space character.
//
