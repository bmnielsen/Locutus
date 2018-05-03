namespace Launcher
{
    using System;
    using System.Collections.Generic;
    using System.Diagnostics;
    using System.Globalization;
    using System.IO;
    using System.Linq;
    using System.Threading;

    public class Program
    {
        private const string BaseDir = "C:\\Users\\bmn\\AppData\\Roaming\\scbw";

        private const int ShortTimeout = 60;
        private const int MediumTimeout = 300;
        private const int LongTimeout = 600;
        private const int MaxTimeout = 1800;

        private static readonly Random Rnd = new Random();

        private static readonly List<string> Maps = new List<string>
                                                        {
                                                            "(2)Benzene.scx",
                                                            "(2)Destination.scx",
                                                            "(2)Heartbreak Ridge.scx",
                                                            "(3)Neo Moon Glaive.scx",
                                                            "(3)Tau Cross.scx",
                                                            "(4)Andromeda.scx",
                                                            "(4)Circuit Breaker.scx",
                                                            "(4)Electric Circuit.scx",
                                                            "(4)Empire of the Sun.scm",
                                                            "(4)Fighting Spirit.scx",
                                                            "(4)Icarus.scm",
                                                            "(4)Jade.scx",
                                                            "(4)La Mancha1.1.scx",
                                                            "(4)Python.scx",
                                                            "(4)Roadrunner.scx"
                                                        };

        private static readonly Dictionary<string, string> LogCache = new Dictionary<string, string>();

        private static string currentGameId;

        private static int wins;

        private static int losses;

        public static void Main(string[] args)
        {
            Go(args);

#if DEBUG
            Console.WriteLine();
            Console.WriteLine("Done!");
            Console.ReadKey();
#endif
        }

        public static void Go(string[] args)
        {
            var opponent = args[0];
            var isHeadless = !args.Contains("ui");
            var showReplay = args.Contains("replay");

            var timeout = MaxTimeout;
            if (args.Contains("short"))
            {
                timeout = ShortTimeout;
            }
            if (args.Contains("medium"))
            {
                timeout = MediumTimeout;
            }
            if (args.Contains("long"))
            {
                timeout = LongTimeout;
            }

            if (args.Contains("latest"))
            {
                File.Copy("C:\\Dev\\BW\\Locutus\\Steamhammer\\bin\\Locutus.dll", $"{BaseDir}\\bots\\Locutus\\AI\\Locutus.dll", true);
            }

            if (args.Contains("clean"))
            {
                ClearDirectory($"{BaseDir}\\bots\\Locutus\\read");
                ClearDirectory($"{BaseDir}\\bots\\Locutus\\write");
                ClearDirectory($"{BaseDir}\\bots\\{opponent}\\read");
                ClearDirectory($"{BaseDir}\\bots\\{opponent}\\write");
            }

            foreach (var arg in args.Where(x => x != "ui"))
            {
                foreach (var map in Maps)
                {
                    if (CultureInfo.InvariantCulture.CompareInfo.IndexOf(map, arg, CompareOptions.IgnoreCase) >= 0)
                    {
                        Run(opponent, map, isHeadless, showReplay, timeout);
                        return;
                    }
                }
            }

            var shuffledMaps = ShuffledMaps();

            if (args.Contains("all") || args.Contains("five"))
            {
                var count = 0;
                var limit = args.Contains("five") ? 5 : shuffledMaps.Count;

                foreach (var map in shuffledMaps)
                {
                    Run(opponent, map, isHeadless, false, timeout);
                    Console.WriteLine("Score is {0} wins {1} losses", wins, losses);

                    count++;
                    if (count >= limit) break;
                }

                return;
            }

            if (args.Contains("2p"))
            {
                Run(opponent, shuffledMaps.First(x => x.Contains("(2)")), isHeadless, showReplay, timeout);
                return;
            }

            if (args.Contains("3p"))
            {
                Run(opponent, shuffledMaps.First(x => x.Contains("(3)")), isHeadless, showReplay, timeout);
                return;
            }

            if (args.Contains("4p"))
            {
                Run(opponent, shuffledMaps.First(x => x.Contains("(4)")), isHeadless, showReplay, timeout);
                return;
            }

            Run(opponent, shuffledMaps.First(), isHeadless, showReplay, timeout);
        }

        private static void Run(string opponent, string map, bool isHeadless, bool showReplay, int timeout)
        {
            Console.WriteLine("Starting game against {0} on {1}", opponent, map);

            var headless = isHeadless ? "--headless" : string.Empty;
            var timeoutParam = timeout > 0 ? "--timeout " + timeout : string.Empty;
            var args = $"--bots \"Locutus\" \"{opponent}\" --game_speed 0 {headless} --vnc_host localhost --map \"sscai/{map}\" {timeoutParam} --read_overwrite";

            currentGameId = null;

            var process = new Process();
            process.StartInfo.UseShellExecute = false;
            process.StartInfo.RedirectStandardOutput = true;
            process.StartInfo.RedirectStandardError = true;
            process.StartInfo.FileName = "cmd.exe";
            process.StartInfo.Arguments = $"/c C:\\Python3\\Scripts\\scbw.play.exe {args}";
            process.OutputDataReceived += ProcessOutput;
            process.ErrorDataReceived += ProcessOutput;
            process.Start();
            process.BeginOutputReadLine();
            process.BeginErrorReadLine();

            var handledReplay = false;
            while (!process.HasExited)
            {   
                Thread.Sleep(500);

                if (!string.IsNullOrEmpty(currentGameId))
                {
                    CheckLogFile($"{BaseDir}\\bots\\Locutus\\write\\GAME_{currentGameId}_0\\Locutus_ErrorLog.txt", "Err: ");
                    CheckLogFile($"{BaseDir}\\bots\\Locutus\\write\\GAME_{currentGameId}_0\\Locutus_log.txt", "Log: ");
                }

                handledReplay = handledReplay 
                    || HandleReplay($"{BaseDir}\\maps\\replays\\GAME_{currentGameId}_0.rep", opponent, map, showReplay)
                    || HandleReplay($"{BaseDir}\\maps\\replays\\GAME_{currentGameId}_1.rep", opponent, map, showReplay);
            }

            handledReplay = handledReplay
                          || HandleReplay($"{BaseDir}\\maps\\replays\\GAME_{currentGameId}_0.rep", opponent, map, showReplay)
                          || HandleReplay($"{BaseDir}\\maps\\replays\\GAME_{currentGameId}_1.rep", opponent, map, showReplay);
        }

        private static bool HandleReplay(string file, string opponent, string map, bool show)
        {
            if (File.Exists(file))
            {
                var fileName = file.Substring(0, file.Length - 4);
                var mapShortName = map.Substring(3, map.Length - 7);
                var newFileName = $"{fileName}-{opponent}-{mapShortName}.rep";
                File.Move(file, newFileName);

                if (!show)
                {
                    return true;
                }

                Process.Start("chrome.exe", "http://www.openbw.com/replay-viewer/");
                Process.Start(
                    "explorer.exe",
                    $"/select,{newFileName}");
                return true;
            }

            return false;
        }

        private static void CheckLogFile(string path, string prefix)
        {
            if (!File.Exists(path))
            {
                return;
            }

            string content;
            using (var fileStream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite))
            using (var textReader = new StreamReader(fileStream))
            {
                content = textReader.ReadToEnd();
            }

            string lastContent;
            if (LogCache.TryGetValue(path, out lastContent) && lastContent == content)
            {
                return;
            }

            var newContent = content.Substring(lastContent?.Length ?? 0);
            var newLines = newContent.Split('\n').Select(x => x.Trim()).Where(x => !string.IsNullOrEmpty(x));
            foreach (var line in newLines)
            {
                Console.WriteLine($"{DateTime.Now:HH:mm:ss} {prefix}{line}");
            }

            LogCache[path] = content;
        }

        private static void ProcessOutput(object sender, DataReceivedEventArgs e)
        {
            if (string.IsNullOrEmpty(e?.Data))
            {
                return;
            }
            
            Console.WriteLine($"{DateTime.Now:HH:mm:ss} {e.Data}");

            if (e.Data.Contains("Waiting until game GAME_"))
            {
                currentGameId = e.Data.Substring(e.Data.IndexOf("GAME_", StringComparison.Ordinal) + 5, 8);
                Console.WriteLine("Got game ID {0}", currentGameId);
            }

            if (e.Data == "0")
            {
                wins++;
            }

            if (e.Data == "1")
            {
                losses++;
            }
        }

        private static List<string> ShuffledMaps()
        {
            var shuffled = new List<string>(Maps);

            var n = shuffled.Count;
            while (n > 1)
            {
                n--;
                var k = Rnd.Next(n + 1);
                var value = shuffled[k];
                shuffled[k] = shuffled[n];
                shuffled[n] = value;
            }

            return shuffled;
        }

        private static void ClearDirectory(string directory)
        {
            try
            {
                var di = new DirectoryInfo(directory);
                foreach (var file in di.EnumerateFiles())
                {
                    file.Delete();
                }
            }
            catch (Exception exception)
            {
                Console.WriteLine("Failed to clear {0}: {1}", directory, exception.Message);
            }
        }
    }
}
