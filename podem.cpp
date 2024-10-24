/*
 Author: Chibudem [Christian] Offodile
 Date last modified: 11/25/2023
 Class: ECE 6140-A

 Description:
 Fault-oriented test vector generation function based on the PODEM (Path Oriented Decision Making) test generation
 algorithm [for digital logic circuits]. The inputs to the program are a logic circuit and a target fault within the
 circuit. The logic circuit will be input in a txt file but the txt file will be processed by a separate function and
 passed into this function as a linked list of gates. The target fault will be input from a txt file (targetFaultRead
 function can read target faults from an input file).
 The output of the function is a pointer to a test vector that would detect the target fault.
*/

#include "podem.h"

// GLOBAL VARIABLES
bool errorAtPO; // initialize to false
int8_t fLine; // If l has value v, l s-a-v is undetectable, initialize to x (-1)
list<gate> gateList;
fault tFault (false, 0);
list<gate*> DFrontier;
unsigned int recCount = 0; // Recursion counter
list<pair<unsigned, int8_t>> testVector;

// PODEM FUNCTION DEFINITIONS

list<pair<unsigned, int8_t>>* PODEM (list<gate>* pGateList, const vector<unsigned>* pInWires, const fault* pFault) {
    recCount++;
    // If it is the first call set up the global variables and increment the recursion counter.
    if (pGateList && pInWires && pFault) {
        gateList = *pGateList; // Make a copy of the circuit then throw away the pointer.
        pGateList = nullptr;
        tFault = *pFault; // Make a copy of the target fault
        errorAtPO = false;
        fLine = -1;
        DFrontier.clear();
        testVector.clear();

        for (int i = 0; i < pInWires->size(); i++) {
            // Add each input wire to the testVector in an invalidated (X) -> (-1) state.
            pair<unsigned int, int8_t> newIn;
            newIn.first = pInWires->at(i);
            newIn.second = -1;
            testVector.push_back(newIn);

            // Find the PIs in the circuit and set their isPI bool to true.
            for (auto g = gateList.begin(); g != gateList.end(); g++) {
                g->setPI(pInWires->at(i)); // setPI does nothing if none of the gate's input wires match the wire ID that was passed in.

                // Mark the target fault wire in the circuit using setFaultSim (only needs to be done once).
                // setFaultSim does nothing if none of the gate's wires match the wire ID that was passed in.
                if (i == pInWires->size()-1) {
                    unsigned int faultWire = tFault.getFaultWire();
                    if ((g->getOutputID() == faultWire) || g->isPInput(faultWire)) g->setFaultSim(faultWire);
                }
            }
        }
    }

    else if (recCount == 1) { // 1st call but not all pointers were passed
        cout << "PODEM called in main with invalid arguements" << endl; // Some arguements were NULL for the first call to PODEM (in main).
        return nullptr;
    }

    /// DEBUG - Break out of possible infinite recursion
    if (recCount > gateList.size()*10) {
        cout << "PODEM crash, recursion count: " << recCount << endl;
        return nullptr;
    }

    if (errorAtPO) return &testVector;

    // The D frontier will be empty on the 1st call because imply has not been called yet. If the fault was not excited
    // or it was excited but cannot be propagated, return false.
    if (recCount > 1) {
        if ((fLine == tFault.getFault()) || ((fLine == !tFault.getFault()) && DFrontier.empty())) return nullptr;
    }

    pair<unsigned int, bool> goal = objective();
    pair<unsigned int, bool> PI = backtrace(goal.first, goal.second);
    list<fault> emptyList; // Empty fault list for non recursive call to imply
    imply (PI.first, PI.second, true, emptyList);
    cout << "Corresponding input assignment: Wire: " << PI.first << " = logic " << PI.second << endl;

    // If imply was successful, change the value of the wire in the testvector list and return it.
    if (PODEM(nullptr, nullptr, nullptr)) {
        for (auto piTest = testVector.begin(); piTest != testVector.end(); piTest++) {
            if (piTest->first == PI.first) piTest->second = PI.second;
        }
        if (pInWires && pFault) recCount = 0; // If this was the first PODEM call (from main) reset recCount.
        return &testVector;
    }

    /* Reverse decision and check (run PODEM). */
    imply (PI.first, false, false, emptyList); // invalidate path before re-simulating with flipped bit.
    imply (PI.first, !PI.second, true, emptyList);
    cout << "Corresponding input assignment: Wire: " << PI.first << " = logic " << !PI.second << endl;
    if (PODEM(nullptr, nullptr, nullptr)) {
        for (auto piTest = testVector.begin(); piTest != testVector.end(); piTest++) {
            if (piTest->first == PI.first) piTest->second = !PI.second;
        }
        if (pInWires && pFault) recCount = 0;
        return &testVector;
    }

    imply (PI.first, false, false, emptyList); // Run forward "invalidation simulation"
    if (pInWires && pFault) recCount = 0;
    return nullptr;
}

//
pair<unsigned int, bool> objective() {
    pair<unsigned int, bool> obj;
    if (fLine == -1) { // return (l, !v) if l is x
        obj.first = tFault.getFaultWire();
        obj.second = !tFault.getFault();
        cout << "Recursion Level: " << recCount << "\nObjective: Wire: " << obj.first << " = logic " << obj.second << endl;
        return obj;
    }

    /// DEBUG
    if (DFrontier.empty()) {
        cout << "Objective called with an empty D-Frontier and l != x." << endl;
        return obj;
    }

    gate D = *DFrontier.front();
    obj.first = D.getInputInv();
    obj.second = !D.cValue();
    cout << "Recursion Level: " << recCount << "\nObjective: Wire: " << obj.first << " = logic " << obj.second << endl;
    return obj;
}


/*
 * Backtrace Pseudocode:
 * In an infinite loop, search through the gateList for the objective wireID. If the objective wireID is a primary
 * input, return the PI so the imply function can simulate that PI. If it's not a PI, the objective wireID must
 * correspond to the output of a gate (not fanout input nodes) for backtracing to be possible. There should only be one
 * output gate corresponding to this wireID. Once this output gate is found, check for an invalid input on the gate and
 * use the wire number of that input as the next "X line" to continue backtracing from. Compute the new value based on
 * the inversion parity of the gate and break the for loop. The while loop restarts the for loop but this time with a
 * new wire ID and value and everything repeats until a primary input is reached.
 * If the X path tracing does not converge, there are two possibilities.
 * One is that the output gate was found but had no invalid inputs for backtracing. The second is that some bug was
 * encountered that caused the loop to run infinitely, the counter will end the loop after an "excesiive" number of
 * iterations.
 */
pair<unsigned int, bool> backtrace (unsigned int wireID, bool value) {
    pair<unsigned int, bool> PIWire;
    PIWire.first = wireID;
    PIWire.second = value;
    unsigned int count = 0;

    // "Infinite loop"
    while (count < gateList.size()) {
        // Find wire in ckt list.
        for (auto g = gateList.begin(); g != gateList.end(); g++) {
            if (g->isPInput(PIWire.first)) { // If the wire is a primary input
                return PIWire;
            }

            if (g->getOutputID() == PIWire.first) { // If the wire is the output wire of g.
                // Check if any of g's inputs are invalid (X) and use that input as the next wire ID.
                unsigned int tempWire = g->getInputInv();
                if (tempWire) { // 0 is not a valid wire ID so a return of 0 means failure
                    PIWire.first = tempWire; // use this "X line" to continue backtracing.
                    PIWire.second = PIWire.second ^ g->invParity(); // v = v xor i
                    break; // Since I found the gate I can break the for loop.
                }

                else { // This should never happen if the other functions work properly.
                    cout << "Found the correct wire node, but there is no X path to a PI." << endl;
                    return PIWire;
                }
            }
        }
        count++;
    }

    cout << "Backtrace could not converge quickly." << endl;
    return PIWire;
}

/*
 * Imply Pseudocode:
 * imply needs to update most global variables (errorAtPO, fLine, gateList, DFrontier, and testVector). Imply is
 * responsible for every value assignment (simulation) in the ckt and it "creates" the initial D/!D when a fault is
 * activated. Imply maintains the D-Frontier based on a 5-valued simulation (between 0, 1, x, D, and !D).
 * Inputs: PI wire and value to set the PI wire to.
 * Outputs: No outputs but changes several global variables.
 * When to stop: If the fault is propagated to a primary output or if it is no longer possible to do so.
 */
void imply (unsigned int wireID, bool value, bool valid, const list<fault> &fList) {
    unsigned int tfaultID = tFault.getFaultWire();
    for (auto gate = gateList.begin(); gate != gateList.end(); gate++) {
        if (valid) {
            if (!gate->hasSimmed) {
                if (gate->setInput(value, wireID)) { // Sets an input if it corresponds to the passed in wireID and returns true.
                    if ((gate->isPInput(tfaultID)) && (wireID == tfaultID)) fLine = value; /// If that was a PI fault line that got set, update fLine.

                    if (!gate->ready) { // Other input invalid
                        if (!fList.empty() || (wireID == tfaultID)) DFrontier.push_back(gate->getAddr()); // Other input invalid plus fault on wire = DFrontier.
                        else if (value == gate->cValue()) gate->ready = true; // No fault plus controlling value on input = ready to sim.
                    }
                    if (!fList.empty()) gate->setFaultList(wireID, fList); // Propagate list.
                }

                // once every gate that is set ready and has not been simmed is simmed, imply will stop getting called.
                if (gate->ready) {
                    bool outVal = gate->simOutput(); // Gate simmed, detectable faults pushed to gate output.

                    list<fault> gateFList = gate->getFaultList();
                    if (!gateFList.empty()) {
                        if (gateFList.front().getFault() != tFault.getFault()) {
                            gateFList.clear();
                            gate->setFaultList(gate->getOutputID(), gateFList);
                        }
                    }

                    DFrontier.remove(gate->getAddr()); // Simulated gates in the DFrontier should be removed (If it's not in the DFrontier nothing happens).
                    if (gate->getOutputID() == tfaultID) fLine = outVal; // If the gate that was simmed was the target fault then update fLine.

                    if (gate->isOutGate) {
                        if (!gate->getFaultList().empty()) {
                            errorAtPO = true;
                            cout << "Fault " << gate->getFaultList().front().getFaultWire() << " s-a-" <<
                            gate->getFaultList().front().getFault() << " has propagated to the output!" << endl;
                            return;
                        }
                    }
                    /// BROADCAST THE SIMMED VALUE AND THE FAULT LIST USING A RECURSIVE CALL TO IMPLY.
                    imply(gate->getOutputID(), outVal, valid, gate->getFaultList());
                    if (errorAtPO) return;
                }
            }
        }

        else { // Forward path-specific invalidation simulation
            uint8_t gInval = gate->invalidateWire(wireID); // If the gate has an input wire with given wire ID, function invalidates the wire and returns 1, 2, or 3. Else it returns 0
            if (gInval) {
                if (wireID == tfaultID) {
                    fLine = -1; // Just invalidated fault line (Should only happen if the fault can't be propagated)
                    cout << "Fault line invalidated." << endl;
                }

                if (gInval > 1) { // Output was invalidated so need to propagate.
                    // Other input had the fault, add to DFrontier
                    if (gInval == 2) DFrontier.push_back(gate->getAddr());

                    // Other input did not have the fault, remove from DFrontier
                    else if (gInval == 3) DFrontier.remove(gate->getAddr());

                    // Propagate invalidation (gate->getFaultList() is empty because the output was invalidated).
                    imply(gate->getOutputID(), false, false, gate->getFaultList());
                }
            }
        }
    }
}

