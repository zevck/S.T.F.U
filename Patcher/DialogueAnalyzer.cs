using Mutagen.Bethesda;
using Mutagen.Bethesda.Synthesis;
using Mutagen.Bethesda.Skyrim;
using System;
using System.Collections.Generic;
using System.Linq;

namespace STFU
{
    public class DialogueAnalyzer
    {
        public static void RunAnalysis(IPatcherState<ISkyrimMod, ISkyrimModGetter> state)
        {
            Console.WriteLine("STFU Dialogue Analyzer");
            Console.WriteLine("======================\n");

            var subtypeStats = new Dictionary<string, SubtypeInfo>();

            int totalTopics = 0;
            foreach (var topicContext in state.LoadOrder.PriorityOrder.DialogTopic().WinningContextOverrides())
            {
                totalTopics++;
                var topic = topicContext.Record;
                
                string subtype = topic.Subtype.ToString();
                string editorId = topic.EditorID?.ToString() ?? "(no EditorID)";
                int responseCount = topic.Responses?.Count ?? 0;

                if (!subtypeStats.ContainsKey(subtype))
                {
                    subtypeStats[subtype] = new SubtypeInfo { Subtype = subtype };
                }

                subtypeStats[subtype].Count++;
                subtypeStats[subtype].TotalResponses += responseCount;
                
                // Keep first 5 examples
                if (subtypeStats[subtype].Examples.Count < 5)
                {
                    subtypeStats[subtype].Examples.Add(editorId);
                }
            }

            // Sort by count descending
            var sortedStats = subtypeStats.Values.OrderByDescending(s => s.Count).ToList();

            Console.WriteLine($"Total DialogTopics: {totalTopics}\n");
            Console.WriteLine("Breakdown by Subtype:");
            Console.WriteLine("=====================\n");

            foreach (var stat in sortedStats)
            {
                Console.WriteLine($"{stat.Subtype}:");
                Console.WriteLine($"  Topics: {stat.Count}");
                Console.WriteLine($"  Total Responses: {stat.TotalResponses}");
                Console.WriteLine($"  Avg Responses/Topic: {(stat.TotalResponses / (double)stat.Count):F1}");
                Console.WriteLine($"  Examples:");
                foreach (var example in stat.Examples)
                {
                    Console.WriteLine($"    - {example}");
                }
                Console.WriteLine();
            }

            // Category analysis
            Console.WriteLine("\nCategory Summary:");
            Console.WriteLine("=================\n");

            var categories = new Dictionary<string, List<string>>
            {
                ["Combat-Related"] = new List<string> { "Combat", "Taunt", "Attack", "Murder", "Assault", "Flee", "Block", "Bash" },
                ["Detection/Awareness"] = new List<string> { "Alert", "Normal", "Lost", "Detection", "Observe" },
                ["Social"] = new List<string> { "Hello", "Goodbye", "Idle", "Scene" },
                ["Criminal"] = new List<string> { "Steal", "Trespass", "Murder", "Assault" },
                ["Other"] = new List<string> { "Custom", "SharedInfo", "Death", "Hit", "Bleedout" }
            };

            foreach (var category in categories)
            {
                int categoryCount = 0;
                int categoryResponses = 0;

                foreach (var stat in sortedStats)
                {
                    if (category.Value.Any(keyword => stat.Subtype.Contains(keyword)))
                    {
                        categoryCount += stat.Count;
                        categoryResponses += stat.TotalResponses;
                    }
                }

                Console.WriteLine($"{category.Key}: {categoryCount} topics, {categoryResponses} responses");
            }

            Console.WriteLine("\nAnalysis complete. No ESP will be generated.");
        }

        private class SubtypeInfo
        {
            public string Subtype { get; set; } = "";
            public int Count { get; set; }
            public int TotalResponses { get; set; }
            public List<string> Examples { get; set; } = new List<string>();
        }
    }
}
