/*
 Author: Chibudem [Christian] Offodile
 Date last modified: 11/04/2023
 Class: ECE 6140-A

 Description:
 This is a program that takes in the description of a logic circuit from a txt file and can simulate the circuit with a
 set of inputs (test vectors). There are four txt files included in this project directory where the logic circuits will
 be pulled from. Each circuit will be simulated with 5 test vectors that are predetermined for each circuit.
 The program is also a deductive fault simulator which, if given a test vector, can output all the faults (bar input
 node faults) that the test vector detects.
*/

#include <iostream>
#include <sstream>
#include <fstream>
#include <random>
#include <cmath>
#include <set>
#include "Classes.h"
#include "podem.h"

using namespace std;

// FUNCTION DECLARATIONS
void simCircuit (const string& txt, vector<vector<bool>> &testV);
void fileRead (const string& file);
void applyInput (vector<vector<bool>> &cktIn, int n);
void broadCast (unsigned int wireID, bool outBit, const list<fault> &fList);
bool targetFaultRead (const string& txt);
vector<bool> randomVector (unsigned int numBits);
void printVector (vector<bool> inVector);
void callPODEM (string& cktFile);
void readVector();

// GLOBAL VARIABLES
list<gate> youngGates; // Gates that are just created (so not ready) are added here.
list<gate> readyGates; // Gates with all inputs ready are added here to be simulated.
vector<gate> outGates; // Output wire values to print at the end of the simulation.
vector<unsigned int> inWires; // inWires[i] needs to correspond to cktInput1[j][i] for ease of setting inputs.
bool simFlag; // If the user inputs 'a' then simFlag is true, else it's false.
bool pFlag; // If pFlag == true, PODEM will be run
vector<pair<unsigned int, bool>> bFaults; // When the user enters b, these are the wires who's faults will be deductively simmed.
set<pair<unsigned int, bool>> setFaults; // takes the detected faults, deleted duplicates and arranges them in ascending order.
fstream wStream;
vector<vector<bool>> cktInput; // Circuit input vector for PODEM

int main() {
    string uIN, uIN2, cktName;

    cout << "Please enter the name of the file containing the circuit that will be simulated." << endl;
    getline (cin, cktName);

    cout << "Enter 'a' to automatically simulate all the nets in the circuit, ";
    cout << "or enter 'b' to simulate only the faults included in the fault input file (infault.txt)." << endl;
    getline (cin, uIN);

    while (true) {
        if (uIN[0] == 'a' && uIN.size() == 1) {
            simFlag = true;
            cout << "Enter the test vectors you'd like to use in the userVector.txt file. Enter any character when you're "
                    "done. If the file is empty, random test vectors will be generated and used to simulate all the "
                    "faults in the circuit. " << endl;

            break;
        }
        else if (uIN[0] == 'b' && uIN.size() == 1) {
            simFlag = false;
            if (!targetFaultRead("infault.txt")) return 0;

            cout << "Enter the test vectors you'd like to use in the userVector.txt file. Enter any character when you're "
                    "done. If the file is empty, PODEM will be used to generate test vectors for each fault in the "
                    "infaults.txt file, and the deductive fault simulator will be run with each test vector. Note that "
                    "only the faults included in the infault.txt file will be detected in these simulations." << endl;

            break;
        }
        else {
            cout << "Invalid input, please choose a or b" << endl;
            getline (cin, uIN);
        }
    }

    wStream.open("outputfile.txt", ios::out);

    fileRead(cktName);
    cin >> uIN2; // Waits for user to finish entering input vectors beforing reading them.
    readVector();
    if (pFlag) callPODEM(cktName);
    else simCircuit(cktName, cktInput);

    wStream.close();
    return 0;
}

// FUNCTION DEFINITIONS

bool targetFaultRead (const string& txt) {
    fstream stream;
    stream.open(txt, ios::in);
    if (stream.is_open()) {
        string input;
        static pair<unsigned, bool> tempFault;

        while (getline(stream, input)) {
            istringstream inStream(input);
            int i = 0, tempInt = 0;

            while (getline(inStream, input, ' ')) {
                if (i == 0) { // The 1st value in a new line should be the wire ID, the 2nd value is the stuck at value.
                    istringstream(input) >> tempInt;
                    if (tempInt < 1) {
                        cout << "Invalid wire number from input file." << endl;
                        return false;
                    }
                    tempFault.first = tempInt;
                }
                else if (i == 1) {
                    istringstream(input) >> tempInt;
                    if (tempInt == 0 || tempInt == 1) {
                        tempFault.second = tempInt;
                        bFaults.push_back(tempFault);
                    }
                    else {
                        cout << "Invalid stuck at value from input file." << endl;
                        return false;
                    }
                }
                else { // If there's more than 2 values in a line, the input file format is incorrect.
                    cout << "Incorrect input file format." << endl;
                    return false;
                }
                ++i;
            }

            if (i < 2) { // If i < 2 then there was only 1 value in a new line.
                cout << "Incorrect input file format." << endl;
                return false;
            }
        }
    }

    else {
        cout << "The stream did not open." << endl;
        return false;
    }

    stream.close();
    return true;
}

void simCircuit (const string& txt, vector<vector<bool>>& testV) {
    wStream << "CIRCUIT " << txt << " OUTPUTS:\n";
    if (!testV.empty()) { // Loop to apply each test vector for a given ckt (txt) if we're given a test vectors.
        for (int i = 0; i < testV.size(); i++) {
            if (i > 0 && simFlag) {
                for (auto &yGate : youngGates) yGate.setFaultSim(yGate.getOutputID());
            }

            applyInput(testV, i);
            printVector(testV[i]);
            wStream << "\nFAULTS DETECTED:" << endl;
            for (auto &j: setFaults) wStream << j.first << " stuck at " << j.second << endl;
            wStream << setFaults.size() << " FAULTS WERE DETECTED BY THE APPLIED VECTORS.\n" << endl;
            setFaults.clear(); // Random vectors are not being used so the # of faults detected is counted for each individual vector.
            for (auto &yGate : youngGates) yGate.invalidateGate(); // Reset all gates [wires and faultlists] for the next test vector.
        }
    }

    else { // If we are not given any test vectors, randomly generate vectors to simulate the circuit. Only stop simulating once 90% fault coverage has been reached or exceeded.
        if (!simFlag) { // If the user entered 'b' then a random vector generation test will be useless.
            cout << "A random test generator simulation should not be run when target faults are input." << endl;
            return;
        }

        int n = 0;
        float fCoverage = 0.0, fDet;
        unsigned numIn = inWires.size();
        vector<vector<bool>> rTestV;
        unsigned numFaults = (youngGates.size() * 2) + (numIn * 2); // Only output wires and primary inputs are considered for fault detection.
        cout << "\nCIRCUIT " << txt << " OUTPUTS:\n";
        
        while (fCoverage <= 0.95) {
            if (n > 0) {
                for (auto &yGate : youngGates) yGate.setFaultSim(yGate.getOutputID());
            }

            rTestV.push_back(randomVector(numIn));
            applyInput(rTestV, n);
            fDet = setFaults.size(); // # faults detected
            fCoverage = fDet/numFaults;
            cout << n+1 << " tests resulted in " << fCoverage*100.0 << "% fault coverage." << endl;
            for (auto &yGate : youngGates) yGate.invalidateGate(); // Reset all gates [wires and faultlists] for the next test vector.

            n++;
            if (n > 9999) {
                cout << "\nRandom test generation cannot produce sufficient coverage in a timely manner." << endl;
                break;
            }
        }

        // After applying all the vectors, print all the faults that were detected.
        wStream << "*** RANDOM TEST VECTORS WERE USED ***\n";
        wStream << "\nFAULTS DETECTED:" << endl;
        for (auto &j: setFaults) wStream << j.first << " stuck at " << j.second << endl;
        wStream << setFaults.size() << " FAULTS WERE DETECTED BY THE APPLIED VECTORS.\n" << endl;
        setFaults.clear();
    }
}

// Generate a random number from 0 to [2^numBits - 1], then converts it to binary.
vector<bool> randomVector (unsigned int numBits) {
    int max = pow(2, numBits) - 1;
    vector<bool> randTest;

    random_device r;
    default_random_engine generator(r());
    uniform_int_distribution<int> distribution(0,max);
    int randNum = distribution(generator);  // generates random number in the range 0 to max.
    cout << "\nThe random number is " << randNum << endl;
    // Convert randNum to binary and store in randTest.
    while (randNum > 0) {
        randTest.push_back(randNum % 2); // The endianness is backwards but it doesn't affect randomness, and its faster.
        randNum /= 2;
    }

    // For small numbers, randTest will have a bit count that is less than the number of inputs of the ckt, so add lagging zeros (weird endianness).
    // Vector<bool> packs the bools as bits in memory which causes SEG Faults to not show up. However this cleanup is still needed to prevent unpredictable behavior.
    numBits -= randTest.size();
    while (numBits > 0) { // Can use resize 1 time instead (if its faster).
        randTest.push_back(0);
        numBits--;
    }
    return randTest;
}

void fileRead (const string &file) {
    fstream stream;
    stream.open(file, ios::in);

    if (stream.is_open()) {
        int exNum; // How many wires (numbers) to expect after a specific gate.
        unsigned int tempID;
        vector<unsigned int> wireIDs; // To keep track of wire IDs till the gate they belong to is created.
        eGate track;

        // Read data from the file object and put it into a string.
        string input;
        while (getline(stream, input)) {
            istringstream inStream(input);

            while (getline(inStream, input, ' ')) {
                if (input == "INV") {
                    track = INV; // I just got an inverter, I am still expecting the 2 wires that belong to it.
                    exNum = 2;
                }

                else if (input == "NAND") {
                    track = NAND;
                    exNum = 3;
                }

                else if (input == "NOR") {
                    track = NOR;
                    exNum = 3;
                }

                else if (input == "AND") {
                    track = AND;
                    exNum = 3;
                }

                else if (input == "OR") {
                    track = OR;
                    exNum = 3;
                }

                else if (input == "BUF") {
                    track = BUF;
                    exNum = 2;
                }

                else if (input == "INPUT") {
                    track = INPUT;
                }

                else if (input == "OUTPUT") {
                    track = OUTPUT;
                }

                // input must be a number if all other cases failed.
                // istringstream might create a garbage number which will be stored in tempID.
                else {
                    istringstream(input) >> tempID;
                    if (track == INPUT) {
                        if (input.empty()) continue;
                        if (tempID != -1) inWires.push_back(tempID); // Still marking and setting input wires
                    }

                    else if (track == OUTPUT) {
                        if (input.empty()) continue;
                        if (tempID != -1) {
                            list<gate>::iterator g;
                            for (g = youngGates.begin(); g != youngGates.end(); g++) {
                                if (g->getOutputID() == tempID) g->isOutGate = true;
                            }
                        }
                    }

                    else { // If track is != INPUT or OUTPUT then the number cannot be less than 1.
                        if (tempID < 1) {
                            cout << "Invalid value from the input file." << endl;
                            return;
                        }

                        wireIDs.push_back(tempID); // Store the wire ID
                        exNum--; // Decrement the number of expected wires for this specific gate (track).

                        if (exNum == 0) { // All needed numbers have been received so create the gate.
                            gate newGate(track, wireIDs); // Now that we have all the wires of the gate, create the gate.
                            // If the user entered 'a', all output wires are simmed. Primary inputs will be set in broadCast.
                            if (simFlag) {
                                unsigned outWire;
                                if (newGate.hasTwoIn) outWire = wireIDs[2];
                                else outWire = wireIDs[1];
                                newGate.setFaultSim(outWire);
                            }
                            wireIDs.clear(); // Clear the wire IDs once they have been created and attached to a gate.
                            youngGates.push_back(newGate); // Add the new gate to the list of gates whose inputs aren't ready.
                        }
                    }
                }
            }
        }
        stream.close(); // Close the file object.
    }
    else {
        cout << "The stream did not open." << endl;
        return;
    }
}

// Takes in a reference to the array of input vectors and the index of the current input vector to be accessed.
void applyInput (vector<vector<bool>> &cktIn, int n) {
    // assign all the input wires their appropriate value and then spread those input values to any fanout nodes.
    list<fault> empty; // The initial input values do not come with a propagated list so the fault list parameter of broadcast is empty.
    for (unsigned i = 0; i < inWires.size(); i++) broadCast(inWires[i], cktIn[n][i], empty);

    while (!readyGates.empty()) {
        gate g = readyGates.front();
        bool oBit = g.simOutput();
        broadCast(g.getOutputID(), oBit, g.getFaultList()); // Sims the output of a gate, computes the fault list and THEN retrieves it.
        if (g.isOutGate) {
            if (!simFlag) { // Some primary output gates might have no fanout and therefore their local fault will never be propagated if the user entered 'b'. Their fault must be created directly.
                for (auto bF: bFaults) {
                    if ((bF.first == g.getOutputID()) && (bF.second != oBit)) {
                        fault f(bF.second, bF.first);
                        list<fault> gFL = g.getFaultList();
                        gFL.push_back(f);
                        g.setFaultList(bF.first, gFL);
                        break; // Every wire only has one local fault so break once it is found and added.
                    }
                }
            }
            outGates.push_back(g);
        }
        readyGates.erase(readyGates.begin());
    }

    pair<unsigned int, bool> pFault;
    for (auto &oG: outGates) {
        // Use a set to sort the faults (after storing the faults as pairs) then print the sorted set of faults.
        for (auto &f : oG.getFaultList()) {
            pFault.first = f.getFaultWire();
            pFault.second = f.getFault();
            setFaults.insert(pFault);
        }
    }
    outGates.clear();
}

// The broadCast function updates fanout nodes of wires whose logic values and lists have been computed. It finds all
// the fanout input nodes for a given output wire that has been computed.
void broadCast (unsigned int wireID, bool outBit, const list<fault> &fList) {
    for (auto g = youngGates.begin(); g != youngGates.end(); g++) {
        if (g->getInputID(1) == wireID) {
            if (!simFlag) { // The user entered 'b'.
                for (auto j: bFaults) {
                    if ((j.first == wireID) && (j.second != outBit)) g->setFaultSim(wireID);
                }
                if (!fList.empty()) g->setFaultList(wireID, fList); // Propagate the fault list of the "origin node" that broadCast was called with.
            }
            // fList is only empty when broadcast is called for primary input wires. The initial fault of primary input wires needs to be created.
            // If the user entered 'a', all primary input faults should be simmed.
            else if (fList.empty()) g->setFaultSim(wireID);
            else g->setFaultList(wireID, fList); // Propagate the fault list of the "origin node" that broadCast was called with.

            g->setInput(outBit, wireID);
            if (g->ready) { // Move g to the ready list and delete it from the young list if all of g's inputs are set.
                readyGates.push_back(*g);
                continue; // No need to check the second input since the gate was ready.
            }
        }

        if (g->hasTwoIn) {
            if (g->getInputID(2) == wireID) {
                if (!simFlag) {
                    for (auto j: bFaults) {
                        if ((j.first == wireID) && (j.second != outBit)) g->setFaultSim(wireID);
                    }
                    if (!fList.empty()) g->setFaultList(wireID, fList);
                }
                else if (fList.empty()) g->setFaultSim(wireID);
                else g->setFaultList(wireID, fList);

                g->setInput(outBit, wireID);
                if (g->ready) {
                    readyGates.push_back(*g);
                }
            }
        }
    }
}

void printVector (vector<bool> inVector) {
    wStream << "INPUT VECTOR ";
    for (auto inV: inVector) wStream << inV;
    wStream << " WAS USED\n";
}

void callPODEM (string& cktFile) {
    vector<bool> podemVector;
    for (auto bF: bFaults) {
        // Check if fault wire number makes sense
        bool isValid = false;
        for (auto yg: youngGates) {
            if ((yg.getInputID(1) == bF.first) || (yg.getOutputID() == bF.first)) {
                isValid = true;
                break;
            }
            if (yg.hasTwoIn && (yg.getInputID(2) == bF.first)) {
                isValid = true;
                break;
            }
        }

        if (!isValid) {
            cout << "Invalid wire number for the given circuit." << endl;
            return;
        }

        fault userFault(bF.second, bF.first);

        list<pair<unsigned, int8_t>>* testMatrix = PODEM(&youngGates, &inWires, &userFault);
        if (testMatrix) { // If a vector was returned print it
            wStream << "\nPRINTING TEST VECTOR RETURNED BY PODEM FOR THE FAULT " << bF.first;
            wStream << " s-a-" << bF.second << ":" << endl;
            for (auto tM = testMatrix->begin(); tM != testMatrix->end(); tM++) {
                // cout << "Wire ID: " << tM->first << " Value: ";
                if (tM->second == -1) {
                    wStream << "X";
                    podemVector.push_back(0);
                }
                else if (tM->second) {
                    wStream << 1;
                    podemVector.push_back(1);
                }
                else {
                    wStream << 0;
                    podemVector.push_back(0);
                }
            }

            wStream << endl;

            // Error Checking
            if (testMatrix->size() != inWires.size()) {
                cout << "Error testMatrix is not the same size as inWires." << endl;
                return;
            }

            // Call simCircuit
            cktInput.push_back(podemVector);
            wStream << "\nDEDUCTIVE SIMULATION FOR " << bF.first << " s-a-" << bF.second << endl;
            simCircuit(cktFile, cktInput); /// Deductive fault sim
        }
        else wStream << "PODEM failed, the fault " << bF.first << " s-a-" << bF.second << " is undetectable!" << endl;
        cout << endl;

        podemVector.clear();
        cktInput.clear();
        if (testMatrix) testMatrix->clear();
    }
}

void readVector() {
    FILE* stream;
    int bit, count = 1;
    vector<bool> userVector;

    stream = fopen("userVector.txt", "r");
    if (stream == NULL) {
        printf("NULL\n");
        return;
    }

    while (fscanf (stream, "%1d", &bit) != EOF) {
        userVector.push_back(bit);
        if (count == inWires.size()) {
            cktInput.push_back(userVector);
            userVector.clear();
            count = 0;
        }
        count++;
    }

    if (count != 1) cout << "Incomplete test vector input." << endl;

    // If there was no complete test vector input, PODEM will be used to generate test vectors
    if (cktInput.empty() && !simFlag) pFlag = true;
    else pFlag = false;

    fclose(stream);
}