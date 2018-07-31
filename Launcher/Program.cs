namespace Launcher
{
    using System;
    using System.Collections.Generic;
    using System.Diagnostics;
    using System.Globalization;
    using System.IO;
    using System.Linq;
    using System.Threading;
    using Docker.DotNet;
    using Docker.DotNet.Models;
    using Newtonsoft.Json;

    public class Program
    {
        private const string BaseDir = "C:\\Users\\bmn\\AppData\\Roaming\\scbw";

        private const int ShortTimeout = 60;
        private const int MediumTimeout = 300;
        private const int LongTimeout = 600;
        private const int ExtraLongTimeout = 1200;
        private const int MaxTimeout = 1800;

        private static readonly Random Rnd = new Random();

        private static readonly List<string> Maps = new List<string>
                                                        {
                                                            "sscai/(2)Benzene.scx",
                                                            "sscai/(2)Destination.scx",
                                                            "sscai/(2)Heartbreak Ridge.scx",
                                                            "sscai/(3)Neo Moon Glaive.scx",
                                                            "sscai/(3)Tau Cross.scx",
                                                            "sscai/(4)Andromeda.scx",
                                                            "sscai/(4)Circuit Breaker.scx",
                                                            "sscai/(4)Electric Circuit.scx",
                                                            "sscai/(4)Empire of the Sun.scm",
                                                            "sscai/(4)Fighting Spirit.scx",
                                                            "sscai/(4)Icarus.scm",
                                                            "sscai/(4)Jade.scx",
                                                            "sscai/(4)La Mancha1.1.scx",
                                                            "sscai/(4)Python.scx",
                                                            "sscai/(4)Roadrunner.scx"
                                                        };

        private static readonly List<string> CigMaps = new List<string>
                                                           {
                                                               "cig/(2)BlueStorm1.2.scx",
                                                               "cig/(2)Destination1.1.scx",
                                                               "cig/(2)Hitchhiker1.1.SCX",
                                                               "cig/(2)MatchPoint1.3.scx",
                                                               "cig/(2)NeoChupungRyeong2.1.scx",
                                                               "cig/(2)NeoHeartbreakerRidge.scx",
                                                               "cig/(2)RideofValkyries1.0.scx",
                                                               "cig/(3)Alchemist1.0.scm",
                                                               "cig/(3)GreatBarrierReef1.0.scx",
                                                               "cig/(3)NeoAztec2.1.scx",
                                                               "cig/(3)Pathfinder1.0.scx",
                                                               "cig/(3)Plasma1.0.scx",
                                                               "cig/(3)TauCross1.1.scx",
                                                               "cig/(4)Andromeda1.0.scx",
                                                               "cig/(4)ArcadiaII2.02.scx",
                                                               "cig/(4)CircuitBreakers1.0.scx",
                                                               "cig/(4)FightingSpirit1.3.scx",
                                                               "cig/(4)LunaTheFinal2.3.scx",
                                                               "cig/(4)NeoSniperRidge2.0.scx",
                                                               "cig/(4)Python1.3.scx"
                                                           };

        private static readonly Dictionary<string, string> LogCache = new Dictionary<string, string>();

        private static DockerClient dockerClient;

        // State for current game
        private static GameData currentGame;

        private static int wins;

        private static int losses;

        private static int crashes;

        private static int timeouts;

        private static StreamWriter logFile = null;

        public static void Main(string[] args)
        {
            dockerClient = new DockerClientConfiguration(new Uri("npipe://./pipe/docker_engine"))
                .CreateClient();

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
            var noOverwrite = args.Contains("noReadOverwrite");

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
            if (args.Contains("extralong"))
            {
                timeout = ExtraLongTimeout;
            }

            if (args.Contains("latest"))
            {
                try
                {
                    File.Copy("C:\\Dev\\BW\\Locutus\\Steamhammer\\bin\\Locutus.dll", $"{BaseDir}\\bots\\Locutus\\AI\\Locutus.dll", true);
                    File.Copy("C:\\Dev\\BW\\Locutus\\Locutus.json", $"{BaseDir}\\bots\\Locutus\\AI\\Locutus.json", true);
                }
                catch (Exception)
                {
                    Console.WriteLine("Could not copy latest version, file may be in use");
                    return;
                }
            }

            if (args.Contains("clean"))
            {
                ClearDirectory($"{BaseDir}\\bots\\Locutus\\read");
                ClearDirectory($"{BaseDir}\\bots\\Locutus\\write");
                ClearDirectory($"{BaseDir}\\bots\\{opponent}\\read");
                ClearDirectory($"{BaseDir}\\bots\\{opponent}\\write");
            }

            var maps = args.Contains("cig") ? CigMaps : Maps;

            foreach (var arg in args.Where(x => x != "ui" && x != "cig"))
            {
                var map = maps.FirstOrDefault(
                    x => CultureInfo.InvariantCulture.CompareInfo.IndexOf(x, arg, CompareOptions.IgnoreCase) >= 0);
                if (map != null)
                {
                    maps = new List<string> { map };
                }
            }

            if (args.Contains("2p"))
            {
                maps = maps.Where(x => x.Contains("(2)")).ToList();
            }
            else if (args.Contains("3p"))
            {
                maps = maps.Where(x => x.Contains("(3)")).ToList();
            }
            else if (args.Contains("4p"))
            {
                maps = maps.Where(x => x.Contains("(4)")).ToList();
            }

            var shuffledMaps = Shuffle(maps);

            if (args.Contains("trainingrun"))
            {
                List<string> trainingOpponents;
                string outputFilename;
                if (File.Exists(opponent))
                {
                    trainingOpponents = File.ReadAllLines(opponent)
                        .Where(x => !string.IsNullOrWhiteSpace(x) && !x.StartsWith("-"))
                        .ToList();
                    outputFilename = "trainingrun-" + Path.GetFileNameWithoutExtension(opponent) + "-" + DateTime.Now.ToString("yyyy-MM-dd-HH-mm-ss");
                }
                else
                {
                    trainingOpponents = new List<string> { opponent };
                    outputFilename = "trainingrun-" + opponent + "-" + DateTime.Now.ToString("yyyy-MM-dd-HH-mm-ss");
                }

                logFile = File.CreateText(outputFilename + ".log");
                var resultsFilename = outputFilename + ".csv";
                File.AppendAllText(resultsFilename, "Opponent;Map;Game ID;My Strategy;Expected Opponent Strategy;Observed Opponent Strategy;Result\n");

                while (true)
                {
                    // Pick an opponent and map at random
                    var trainingOpponent = trainingOpponents[Rnd.Next(0, trainingOpponents.Count)].Split(';');
                    var map = shuffledMaps[Rnd.Next(0, shuffledMaps.Count)];

                    // Set the timeout
                    timeout = MaxTimeout;
                    if (trainingOpponent.Length > 1)
                    {
                        if (trainingOpponent[1] == "short")
                        {
                            timeout = ShortTimeout;
                        }
                        else if (trainingOpponent[1] == "medium")
                        {
                            timeout = MediumTimeout;
                        }
                        else if (trainingOpponent[1] == "long")
                        {
                            timeout = LongTimeout;
                        }
                        else if (trainingOpponent[1] == "extralong")
                        {
                            timeout = ExtraLongTimeout;
                        }
                    }

                    Run(trainingOpponent[0], map, true, false, timeout, noOverwrite);

                    var result = currentGame.HaveResult ? (currentGame.Won ? "win" : "loss") : "crash";
                    File.AppendAllText(resultsFilename, $"{trainingOpponent[0]};{map};{currentGame.Id};{currentGame.MyStrategy};{currentGame.ExpectedOpponentStrategy};{currentGame.OpponentStrategy};{result}\n");

                    Output("Overall score is {0} wins {1} losses {2} crashes {3} timeouts", wins, losses, crashes, timeouts);
                    logFile.Flush();
                }
            }

            if (args.Contains("all") || args.Contains("five"))
            {
                var count = 0;
                var limit = args.Contains("five") ? 5 : shuffledMaps.Count;

                foreach (var map in shuffledMaps)
                {
                    Run(opponent, map, isHeadless, false, timeout, noOverwrite);

                    Output("Score is {0} wins {1} losses {2} crashes {3} timeouts", wins, losses, crashes, timeouts);

                    count++;
                    if (count >= limit) break;
                }

                return;
            }

            Run(opponent, shuffledMaps.First(), isHeadless, showReplay, timeout, noOverwrite);
        }

        private static void Run(string opponent, string map, bool isHeadless, bool showReplay, int timeout, bool noOverwrite)
        {
            Output("Starting game against {0} on {1}", opponent, map);

            var headless = isHeadless ? "--headless" : string.Empty;
            var timeoutParam = timeout > 0 ? "--timeout " + timeout : string.Empty;
            var overwriteParam = noOverwrite ? string.Empty : "--read_overwrite";
            var args = $"--bots \"Locutus\" \"{opponent}\" --game_speed 0 {headless} --vnc_host localhost --map \"{map}\" {timeoutParam} {overwriteParam}";

            currentGame = new GameData();

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

            while (!process.HasExited)
            {   
                Thread.Sleep(500);

                if (logFile != null)
                {
                    logFile.Flush();
                }

                if (string.IsNullOrEmpty(currentGame.Id))
                {
                    continue;
                }

                // Output our log to console
                CheckLogFile($"{BaseDir}\\bots\\Locutus\\write\\GAME_{currentGame.Id}_0\\Locutus_ErrorLog.txt", "Err: ");
                CheckLogFile($"{BaseDir}\\bots\\Locutus\\write\\GAME_{currentGame.Id}_0\\Locutus_log.txt", "Log: ");

                // Process output files
                ProcessOutputFiles(opponent, map, showReplay);

                // Check for crashes or games that hang after completion
                if (IsGameOver(opponent))
                {
                    try
                    {
                        process.OutputDataReceived -= ProcessOutput;
                        process.ErrorDataReceived -= ProcessOutput;
                        process.Kill();
                    }
                    catch (Exception)
                    {
                        // Ignore exceptions, it can throw if the process exited in the meantime
                    }
                }
            }

            ProcessOutputFiles(opponent, map, showReplay);

            if (!currentGame.HaveResult)
            {
                Output("Result: Timeout");
                timeouts++;
            }
            else if (currentGame.Won)
            {
                Output("Result: Won");
                wins++;
            }
            else if (currentGame.Crashed)
            {
                Output("Result: Crashed");
                crashes++;
            }
            else
            {
                Output("Result: Loss");
                losses++;
            }

            KillContainers();
        }

        private static void ProcessOutputFiles(string opponent, string map, bool showReplay)
        {
            // Parse the results files
            HandleResults($"{BaseDir}\\logs\\GAME_{currentGame.Id}_0_results.json", true);
            HandleResults($"{BaseDir}\\logs\\GAME_{currentGame.Id}_1_results.json", false);

            // Rename the replay file as soon as we see it after the game is over
            if (currentGame.HaveResult)
            {
                currentGame.RenamedReplay = currentGame.RenamedReplay
                                            || HandleReplay($"{BaseDir}\\maps\\replays\\GAME_{currentGame.Id}_0.rep", opponent, map, showReplay)
                                            || HandleReplay($"{BaseDir}\\maps\\replays\\GAME_{currentGame.Id}_1.rep", opponent, map, showReplay);
            }
        }

        private static void HandleResults(string file, bool mine)
        {
            if (currentGame.HaveResult && (!mine || !currentGame.Crashed))
            {
                return;
            }

            var resultsFileContent = ReadFileContents(file);
            if (string.IsNullOrEmpty(resultsFileContent))
            {
                return;
            }

            var results = JsonConvert.DeserializeObject<GameResult>(resultsFileContent);
            if (!results.IsCrashed)
            {
                currentGame.HaveResult = true;
                currentGame.ResultTimestamp = DateTime.UtcNow;
                currentGame.Won = mine ? results.IsWinner : !results.IsWinner;
                if (mine)
                {
                    currentGame.Crashed = false;
                }
            }
            else if (mine)
            {
                currentGame.Crashed = true;
            }
        }

        private static bool HandleReplay(string file, string opponent, string map, bool show)
        {
            if (File.Exists(file))
            {
                var fileName = file.Substring(0, file.Length - 4);
                var mapShortName = map.Split('/')[1];
                mapShortName = mapShortName.Substring(3, mapShortName.Length - 7);
                var result = currentGame.Won ? "win" : "loss";
                var newFileName = $"{fileName}-{opponent}-{mapShortName}-{result}.rep";

                try
                {
                    File.Move(file, newFileName);
                }
                catch (Exception)
                {
                    // File might be locked, try again later
                    return false;
                }

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
            foreach (var line in GetNewLinesFromFile(path))
            {
                if (line.Contains(": Strategy: "))
                {
                    currentGame.MyStrategy = line.Substring(line.IndexOf(": Strategy: ", StringComparison.InvariantCulture) + 12);
                }

                if (line.Contains(": Opponent: "))
                {
                    var strategy = line.Substring(line.IndexOf(": Opponent: ", StringComparison.InvariantCulture) + 12);
                    if (strategy.StartsWith("expect "))
                    {
                        currentGame.ExpectedOpponentStrategy = strategy.Substring(7);
                    }
                    else
                    {
                        currentGame.OpponentStrategy = strategy;
                    }
                }

                Output($"{DateTime.Now:HH:mm:ss} {prefix}{line}");
            }
        }

        private static void ProcessOutput(object sender, DataReceivedEventArgs e)
        {
            if (string.IsNullOrEmpty(e?.Data))
            {
                return;
            }
            
            Output($"{DateTime.Now:HH:mm:ss} {e.Data}");

            if (e.Data.Contains("Waiting until game GAME_"))
            {
                currentGame.Id = e.Data.Substring(e.Data.IndexOf("GAME_", StringComparison.Ordinal) + 5, 8);
                Output("Got game ID {0}", currentGame.Id);
            }
        }

        private static bool IsGameOver(string opponent)
        {
            // Check logs for repeated "waiting for players" messages
            var myLogFilename = $"{BaseDir}\\logs\\GAME_{currentGame.Id}_0_Locutus_game.log";
            var opponentLogFilename = $"{BaseDir}\\logs\\GAME_{currentGame.Id}_0_{opponent.Replace(' ', '_')}_game.log";
            foreach (var line in GetNewLinesFromFile(myLogFilename).Concat(GetNewLinesFromFile(opponentLogFilename)))
            {
                if (line == "waiting for players...")
                {
                    currentGame.WaitingForPlayersCount++;

                    if (currentGame.WaitingForPlayersCount > 2)
                    {
                        Output("Killing game as it appears to have crashed");
                        return true;
                    }
                }
                else
                {
                    currentGame.WaitingForPlayersCount = 0;
                }
            }

            // Kill if it has been 20 seconds since we got a result
            if (currentGame.HaveResult && (DateTime.UtcNow - currentGame.ResultTimestamp).TotalSeconds > 20)
            {
                return true;
            }

            return false;
        }

        private static IEnumerable<string> GetNewLinesFromFile(string path)
        {
            var content = ReadFileContents(path);
            if (string.IsNullOrEmpty(content))
            {
                return new List<string>();
            }

            string lastContent;
            if (LogCache.TryGetValue(path, out lastContent) && lastContent == content)
            {
                return new List<string>();
            }

            var newContent = content.Substring(lastContent?.Length ?? 0);
            var newLines = newContent.Split('\n').Select(x => x.Trim()).Where(x => !string.IsNullOrEmpty(x));

            LogCache[path] = content;

            return newLines;
        }

        private static string ReadFileContents(string path)
        {
            if (!File.Exists(path))
            {
                return null;
            }

            try
            {
                // This should allow us to read the file in most cases even if it is being written
                using (var fileStream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite))
                using (var textReader = new StreamReader(fileStream))
                {
                    return textReader.ReadToEnd();
                }
            }
            catch (Exception)
            {
                // We couldn't read the file for some reason, just ignore it for now
            }

            return null;
        }

        private static List<string> Shuffle(List<string> maps)
        {
            var shuffled = new List<string>(maps);

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

        private static void Output(string format, params object[] args)
        {
            if (args.Any())
            {
                try
                {
                    Console.WriteLine(format, args);
                    logFile?.WriteLine(format, args);
                    return;
                }
                catch (FormatException)
                {
                    // Fall through and output the raw text
                }
            }

            Console.WriteLine(format);
            logFile?.WriteLine(format);
        }

        private static void KillContainers()
        {
            Thread.Sleep(1000);

            var runningContainers = dockerClient.Containers.ListContainersAsync(
                new ContainersListParameters
                    {
                        All = true,
                        Filters = 
                            new Dictionary<string, IDictionary<string, bool>>
                                {
                                    {
                                        "status", new Dictionary<string, bool>
                                                      {
                                                          {
                                                              "running",
                                                              true
                                                          }
                                                      }
                                          }
                                      }
                    }).GetAwaiter().GetResult();

            foreach (var container in runningContainers.Where(x => x.Names.Any(n => n.Contains(currentGame.Id))))
            {
                dockerClient.Containers.StopContainerAsync(container.ID, new ContainerStopParameters());
                Console.WriteLine("Stopped container {0}", container.Names[0]);
            }
        }

        private class GameData
        {
            public string Id { get; set; }

            public int WaitingForPlayersCount { get; set; }

            public bool HaveResult { get; set; }

            public DateTime ResultTimestamp { get; set; }

            public bool Crashed { get; set; }

            public bool Won { get; set; }

            public bool RenamedReplay { get; set; }

            public string MyStrategy { get; set; }

            public string ExpectedOpponentStrategy { get; set; }

            public string OpponentStrategy { get; set; }
        }

        public class GameResult
        {
            [JsonProperty("is_winner")]
            public bool IsWinner { get; set; }

            [JsonProperty("is_crashed")]
            public bool IsCrashed { get; set; }
        }
    }
}
