import subprocess
import os
import threading
import time
import json
import uuid
import re

FRAMEWORK_JAR = "IngeniousFramework.jar"
ADD_OPENS = "--add-opens java.base/java.util=ALL-UNNAMED --add-opens java.desktop/java.awt=ALL-UNNAMED"
GAMERESULTS = []
regexnd = r"INFO: You \(.*?\) (?P<res>.*)"
regexd = r"INFO: It's a draw!"


def testMpichInstalled():
    _, _, exitCode = run_command("mpichversion")
    assert exitCode == 0, "mpich is not installed"


def kill_procs():
    try:
        pids = subprocess.check_output("lsof -t -i:61235", shell=True)
        if pids:
            print("Killing Old Server")
            subprocess.call("kill " + str(pids.decode().strip()), shell=True)
            time.sleep(2)
    except:
        print("No Live Servers Found")
        pass


def run_command(command, print_output=False):
    subproc = subprocess.Popen(
        command,
        shell=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE
    )
    if print_output:
        output = ""
        while subproc.poll() is None:
            line = subproc.stdout.readline()
            output += line.decode()
            if line:
                print(line.decode().strip())
    else:
        output = subproc.stdout.read().decode()
    error = subproc.stderr.read().decode()
    exitCode = subproc.wait()
    subproc.kill()
    return output, error, exitCode


def makePlayer():
    os.chdir("src_my_player")
    run_command("make clean")
    _, _, exitCode = run_command("make")
    assert exitCode == 0, "make failed"
    run_command("make clean")
    os.chdir("..")


def makeRandomPlayer():
    os.chdir("src_random_player")
    run_command("make clean")
    _, _, exitCode = run_command("make")
    assert exitCode == 0, "make random failed"
    run_command("make clean")
    os.chdir("..")


def moveLogs():
    print("Moving log output to the Logs directory")
    run_command("mv *.txt Logs/")


def startServer():
    # java -jar IngeniousFramework.jar server -port 61235
    output, error, exitCode = run_command(
        f"java -jar {ADD_OPENS} {FRAMEWORK_JAR} server -port 61235")


def startLobby(lid):
    output, error, exitCode = run_command(
        f"java -jar {ADD_OPENS} {FRAMEWORK_JAR} create -config \"Othello.json\" -game \"OthelloReferee\" -lobby \"mylobby-" + str(lid) + "\" -hostname localhost -port 61235")


def startPlayer(player, lid):
    printout = False
    if player == "my_player":
        printout = True
    output, error, exitCode = run_command(f"java -jar {ADD_OPENS} {FRAMEWORK_JAR} client -config \"Othello.json\" -username " + str(
        player) + " -engine za.ac.sun.cs.ingenious.games.othello.engines.OthelloMPIEngine -game OthelloReferee -hostname localhost -lobby \"mylobby-" + str(lid) + "\" -port 61235", printout)
    if printout:
        matches = re.findall(regexnd, output, re.MULTILINE)
        if len(matches) == 0:
            matches = re.findall(regexd, output, re.MULTILINE)
            if len(matches) == 0:
                print("Error finding Result!")
            else:
                GAMERESULTS.append(0.5)
                print("GAME WAS A DRAW!")
        else:
            print(matches)
            for match in matches:
                if "won" in match:
                    GAMERESULTS.append(1)
                    print("GAME WAS WON!")
                elif "lost" in match:
                    GAMERESULTS.append(0)
                    print("GAME WAS LOST!")
                else:
                    GAMERESULTS.append(0.5)
                    print("GAME WAS A DRAW!")


def writeGameConf(p1, p2):
    game = {
        "numPlayers": 2,
        "threads": 4,
        "boardSize": 8,
        "time": 4,
        "turnLength": 4000,
        "path1": p1,
        "path2": p2
    }
    json_object = json.dumps(game, indent=4)
    with open("Othello.json", "w") as outfile:
        outfile.write(json_object)


if __name__ == "__main__":
    directory = "players"
    if not os.path.exists(directory):
        os.mkdir(directory)
#    print("Testing mpich...")
#    testMpichInstalled()
    print("Making player...")
    makePlayer()
    print("Making Random Player...")
    makeRandomPlayer()
    kill_procs()
    print("Starting Server and Lobby...")
    t_server = threading.Thread(target=startServer)
    t_server.daemon = True
    t_server.start()
    time.sleep(2)

    for filename in os.listdir(directory):
        f = os.path.join(directory, filename)
        if os.path.isfile(f):
            if (f == "players/my_player"):
                continue

        print("Match of " + f + " vs my_player")
        writeGameConf(f, "players/my_player")
        lid = str(uuid.uuid4())
        t_lobby = threading.Thread(target=startLobby, args=(lid,))
        t_lobby.start()
        time.sleep(2)
        t_player1 = threading.Thread(target=startPlayer, args=(filename, lid,))
        t_player2 = threading.Thread(
            target=startPlayer, args=("my_player", lid,))
        t_player1.start()
        time.sleep(1)
        t_player2.start()
        t_player1.join()
        t_player2.join()
        t_lobby.join()

        print("Match of my_player vs " + f)
        writeGameConf("players/my_player", f)
        lid = str(uuid.uuid4())
        t_lobby = threading.Thread(target=startLobby, args=(lid,))
        t_lobby.start()
        time.sleep(2)
        t_player1 = threading.Thread(
            target=startPlayer, args=("my_player", lid,))
        t_player2 = threading.Thread(target=startPlayer, args=(filename, lid,))
        t_player1.start()
        time.sleep(1)
        t_player2.start()
        t_player1.join()
        t_player2.join()
        t_lobby.join()

    kill_procs()
    moveLogs()
    print("Done")
    print(GAMERESULTS)
