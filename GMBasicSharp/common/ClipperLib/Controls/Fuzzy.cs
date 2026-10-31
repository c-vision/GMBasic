using CommonLib.Consoles.Tui;
using CommonLib.Screens;
using System.Text;

namespace ClipperLib.Controls
{
    public class NameMatch
    {
        public int Score { get; set; }
        public List<int> Positions { get; set; } = [];
    }

    public class FuzzyPattern
    {
        private const int BonusMatch = 50000;
        private const int BonusExact = 1000;
        private const int BonusStart = 10;
        private const int BonusStartWord = 5;
        private const int BonusCandidateLength = -1;
        private const int BonusMatchLength = -10;
        private const int BonusNbHoles = -30;
        private const int BonusSingledChar = -15;

        private readonly char[] _chars;
        private readonly int _maxNbHoles;

        public bool IsEmpty => _chars.Length == 0;

        public string Pattern => new string(_chars);

        public FuzzyPattern(string query)
        {
            _chars = NormalizeLower(query).ToCharArray();
            _maxNbHoles = _chars.Length switch
            {
                1 => 0,
                2 => 1,
                3 => 2,
                4 => 2,
                5 => 2,
                6 => 3,
                7 => 3,
                8 => 4,
                _ => _chars.Length * 4 / 7
            };
        }

        public int? Score(string candidate)
        {
            var match = Find(candidate);
            return match?.Score;
        }

        public bool Matches(string candidate)
        {
            return Find(candidate) != null;
        }

        public NameMatch? Find(string candidate)
        {
            var candChars = NormalizeLower(candidate).ToCharArray();

            if (_chars.Length == 0)
                return new NameMatch { Score = BonusMatch, Positions = [] };

            if (candChars.Length < _chars.Length)
                return null;

            var bestScore = 0;
            NameMatch? bestMatch = null;
            var n = candChars.Length + 1 - _chars.Length;

            for (var startIdx = 0; startIdx < n; startIdx++)
            {
                if (candChars[startIdx] == _chars[0])
                {
                    var result = TightMatchFromIndex(candChars, startIdx);
                    if (result is PerfectMatch perfect)
                        return perfect.Match;
                    if (result is SomeMatch some && some.Match.Score > bestScore)
                    {
                        bestScore = some.Match.Score;
                        bestMatch = some.Match;
                    }
                }
            }

            return bestMatch!;
        }

        public static List<(int Index, int Score)> Filter(string query, IEnumerable<string> items)
        {
            var pat = new FuzzyPattern(query);
            var scored = new List<(int, int)>();
            var idx = 0;
            foreach (var item in items)
            {
                var score = pat.Score(item);
                if (score.HasValue)
                    scored.Add((idx, score.Value));
                idx++;
            }
            scored.Sort((a, b) => b.Item2.CompareTo(a.Item2));
            return scored;
        }

        public static List<(string Text, int Score)> BestMatches(
            string pattern, IEnumerable<string> candidates, int limit)
        {
            var pat = new FuzzyPattern(pattern);
            var results = new List<(string, int)>();

            foreach (var candidate in candidates)
            {
                var score = pat.Score(candidate);
                if (score.HasValue)
                    results.Add((candidate, score.Value));
            }

            results.Sort((a, b) => b.Item2.CompareTo(a.Item2));

            if (limit < results.Count)
                results = results.GetRange(0, limit);

            return results;
        }

        private MatchSearchResult TightMatchFromIndex(char[] candChars, int startIdx)
        {
            var pos = new int[_chars.Length];
            var candIdx = startIdx;
            var patIdx = 0;
            var inHole = false;

            while (true)
            {
                if (candChars[candIdx] == _chars[patIdx])
                {
                    pos[patIdx] = candIdx;

                    if (inHole)
                    {
                        var revIdx = 1;
                        while (true)
                        {
                            if (patIdx < revIdx) break;
                            if (candChars[candIdx - revIdx] == _chars[patIdx - revIdx])
                                pos[patIdx - revIdx] = candIdx - revIdx;
                            else
                                break;
                            revIdx++;
                        }
                        inHole = false;
                    }

                    patIdx++;
                    if (patIdx == _chars.Length) break;
                }
                else
                {
                    if (candChars.Length - candIdx <= _chars.Length - patIdx)
                        return new NoMatch();
                    inHole = true;
                }

                candIdx++;
            }

            var nbHoles = 0;
            var nbSingledChars = 0;

            for (var idx = 1; idx < pos.Length; idx++)
            {
                if (pos[idx] > 1 + pos[idx - 1])
                {
                    nbHoles++;
                    if (idx > 1 && pos[idx - 1] > 1 + pos[idx - 2])
                    {
                        if (candChars[pos[idx - 2] + 1] == candChars[pos[idx - 1]])
                        {
                            pos[idx - 1] = pos[idx - 2] + 1;
                            nbHoles--;
                        }
                        else
                        {
                            nbSingledChars++;
                        }
                    }
                }
            }

            if (nbHoles > _maxNbHoles)
                return new NoMatch();

            var matchLen = 1 + candIdx - pos[0];
            var score = BonusMatch;
            score += BonusCandidateLength * candChars.Length;
            score += BonusSingledChar * nbSingledChars;
            score += BonusNbHoles * nbHoles;
            score += matchLen * BonusMatchLength;

            var match = new NameMatch { Score = score, Positions = new List<int>(pos) };

            if (pos[0] == 0)
            {
                score += BonusStart + BonusStartWord;
                match.Score = score;
                if (candChars.Length == _chars.Length)
                {
                    score += BonusExact;
                    match.Score = score;
                    return new PerfectMatch(match);
                }
            }
            else
            {
                var previous = candChars[pos[0] - 1];
                if (IsWordSeparator(previous))
                {
                    score += BonusStartWord;
                    match.Score = score;
                    if (candChars.Length - pos[0] == _chars.Length)
                        return new PerfectMatch(match);
                }
            }

            match.Score = score;
            return new SomeMatch(match);
        }

        private static bool IsWordSeparator(char c)
        {
            return c is '_' or ' ' or '-';
        }

        private static string NormalizeLower(string s)
        {
            var sb = new StringBuilder(s.Length);
            foreach (var c in s)
            {
                var n = NormalizeLowerChar(c);
                if (n.HasValue)
                    sb.Append(n.Value);
            }
            return sb.ToString();
        }

        private static char? NormalizeLowerChar(char c)
        {
            if (c is >= '\u0300' and <= '\u036F')
                return null;

            var lower = char.ToLowerInvariant(c);
            return StripDiacritic(lower) ?? lower;
        }

        private static char? StripDiacritic(char c)
        {
            return c switch
            {
                'à' or 'á' or 'â' or 'ã' or 'ä' or 'å' or 'ā' or 'ă' or 'ą' => 'a',
                'è' or 'é' or 'ê' or 'ë' or 'ē' or 'ę' => 'e',
                'ì' or 'í' or 'î' or 'ï' or 'ī' => 'i',
                'ò' or 'ó' or 'ô' or 'õ' or 'ö' or 'ø' or 'ō' or 'ő' => 'o',
                'ù' or 'ú' or 'û' or 'ü' or 'ū' or 'ů' or 'ű' => 'u',
                'ñ' or 'ń' or 'ň' => 'n',
                'ç' or 'ć' or 'č' => 'c',
                'ž' or 'ź' or 'ż' => 'z',
                'š' or 'ś' or 'ş' => 's',
                'ý' or 'ÿ' => 'y',
                'ř' or 'ŕ' => 'r',
                'ť' or 'ţ' => 't',
                'ď' or 'đ' => 'd',
                'ľ' or 'ł' => 'l',
                'ğ' or 'ĝ' => 'g',
                _ => null
            };
        }
    }

    internal abstract class MatchSearchResult { }

    internal class PerfectMatch : MatchSearchResult
    {
        public NameMatch Match { get; }
        public PerfectMatch(NameMatch match) { Match = match; }
    }

    internal class SomeMatch : MatchSearchResult
    {
        public NameMatch Match { get; }
        public SomeMatch(NameMatch match) { Match = match; }
    }

    internal class NoMatch : MatchSearchResult { }
}
