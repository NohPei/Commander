#include "Commander.h"

//Initialise the array of internal commands with the constructor
Commander::Commander(): internalCommands ({ "U",
																					  "X",
																					  "help",
																						"?",
																						"echo",
																						"echo aux",
																						"enable",
																						"errors"}){
	bufferString.reserve(bufferSize);
	ports.settings.reg = COMMANDER_DEFAULT_REGISTER_SETTINGS;
	commandState.reg = COMMANDER_DEFAULT_STATE_SETTINGS;
}
//==============================================================================================================
Commander::Commander(uint16_t reservedBuffer): internalCommands ({ 	"U",
																																		"X",
																																		"help",
																																		"?",
																																		"echo",
																																		"echo aux",
																																		"enable",
																																		"errors"}){
	//bufferString.reserve(bufferSize);
	bufferSize = reservedBuffer;
	bufferString.reserve(bufferSize);
	ports.settings.reg = COMMANDER_DEFAULT_REGISTER_SETTINGS;
	commandState.reg = COMMANDER_DEFAULT_STATE_SETTINGS;
}
//==============================================================================================================

void	Commander::begin(Stream *sPort){
	ports.inPort = sPort;
	ports.outPort = sPort;
	//portAttached = true;
	//ports.inPort->println("Port Attached");
	setup();
}
//==============================================================================================================
void	Commander::begin(Stream *sPort, const commandList_t *commands, uint32_t size){
	ports.inPort = sPort;
	ports.outPort = sPort;
	attachCommands(commands, size);
	resetBuffer();
}
//==============================================================================================================
void	Commander::begin(Stream *sPort, Stream *oPort, const commandList_t *commands, uint32_t size){
	ports.inPort = sPort;
	ports.outPort = oPort;
	attachCommands(commands, size);
	resetBuffer();
}
//==============================================================================================================

bool Commander::update(){
	if(!ports.inPort) return 0; //don't bother if there is no stream attached
	//Check if streamOn is true. If it is then all incoming chars get routed somewhere and no command processing happens (for sending files ...)
	if(commandState.bit.streamOn){
		
		bufferString = "";//clear the buffer so we can fill it with any new chars
		bytesWritten = 0;
		commandState.bit.bufferFull = false;
		
		while(ports.inPort->available()){
			int inByte = ports.inPort->read();
			if(inByte == EOFChar){
				println("EOF Found, tidying up");
				commandState.bit.streamOn = false;
				//get rid of any newlines or CRs in the stream
				while(ports.inPort->peek() == '\n' || ports.inPort->peek() == '\r') ports.inPort->read();
				//call the handler again so it can clean up and close anything that needs closing
				commandState.bit.commandHandled = handleCustomCommand();
				resetBuffer();
				printCommandPrompt();
				return (bool)ports.inPort->available(); //return true if any bytes left to read
			}
			//write incoming data to the buffer
      writeToBuffer(inByte);
			//echo to ports if configured
			echoPorts(inByte);
			//call the handler if you fill the buffer, then return so everything is reset
      if(bytesWritten == bufferSize-1 || !ports.inPort->available()) {
				
				println("Buffer ready, calling handler");
				commandState.bit.commandHandled = handleCustomCommand();
				
				println("Clearing buffer");
				bufferString = "";//clear the buffer so we can fill it with any new chars
				bytesWritten = 0;
				resetBuffer();
				return (bool)ports.inPort->available(); //return true if any bytes left to read
			}
    }
		return (bool)ports.inPort->available(); //return true if any bytes left to read
	}
	//returns true if any bytes left to read
	if(commandState.bit.isCommandPending){
		//there is a command still in the buffer, process it now
		commandState.bit.isCommandPending = false;
		commandState.bit.commandHandled = handleCommand();
		if(ports.inPort) return (bool)ports.inPort->available(); //return true if any bytes left to read
		return 0;
	}

	commandState.bit.commandHandled = false;
	if(ports.settings.bit.commandParserEnabled){
		while(ports.inPort->available()){
			int inByte = ports.inPort->read();
			echoPorts(inByte);
      if(processBuffer(inByte)) break; //break out of here - an end of line was found so unpack and handle the command 
    }
    //copy any pending characters back from the alt ports.inPort
		if(ports.settings.bit.echoToAlt && ports.altPort) while(ports.altPort->available()) { ports.outPort->write(ports.altPort->read()); }

		//If a newline was detected, try and handle the command
    if(commandState.bit.newLine == true) commandState.bit.commandHandled = handleCommand(); //returns true if data was unpacked
	}
	else bridgePorts();
	if(ports.inPort) return (bool)ports.inPort->available(); //return true if any bytes left to read
  return 0;
}

//Echo incoming to out and alt ports
void Commander::echoPorts(int portByte){
	if(ports.settings.bit.echoTerminal && !ports.settings.bit.locked) ports.outPort->write(portByte);
	if(ports.settings.bit.echoToAlt && ports.altPort && !ports.settings.bit.locked) ports.altPort->write(portByte);
}

//copy data between ports
void Commander::bridgePorts(){
	if(!ports.settings.bit.commandParserEnabled && ports.settings.bit.echoToAlt && ports.altPort){
			//pass data between ports
			while(ports.altPort->available()) ports.outPort->write(ports.altPort->read());
			while(ports.inPort->available()) ports.altPort->write(ports.inPort->read());
	}
}
//==============================================================================================================
bool   Commander::updatePending(){
	if(commandState.bit.isCommandPending){
		//there is a command still in the buffer, process it now
		commandState.bit.isCommandPending = false;
		commandState.bit.commandHandled = handleCommand();
		if(ports.inPort) return (bool)ports.inPort->available(); //return true if any bytes left to read
		return 0;
	}
}
//==============================================================================================================


bool Commander::feed(Commander& Cmdr){
	//Feed the payload of a different commander object to this one
	//Copy the String buffer then handle the command
	
	bufferString = Cmdr.bufferString.substring(Cmdr.endIndexOfLastCommand+1);

	disablePrompt(); //dsiable the prompt so it doesn't print twice
	commandState.bit.commandHandled = handleCommand(); //try and handle the command

	if( ports.settings.bit.multiCommanderMode == false ) enablePrompt(); //re-enable the prompt if in single commander mode so it prints on exit
	return commandState.bit.commandHandled;
}
//==============================================================================================================


bool Commander::hasPayload(){
	//returns true if there is anything except an end of line after the command
	if(bufferString.charAt(endIndexOfLastCommand) != endOfLine) return true;
	return false;
}
//==============================================================================================================
String Commander::getPayload(){
	return bufferString.substring(endIndexOfLastCommand+1);
}
//==============================================================================================================
String Commander::getPayloadString(){
	//return the payload minus any newline
	if(bufferString.substring(endIndexOfLastCommand+1).indexOf('\n') > -1){
		return bufferString.substring(endIndexOfLastCommand+1, bufferString.indexOf('\n'));
	}
	return bufferString.substring(endIndexOfLastCommand+1);
}
//==============================================================================================================

bool Commander::feedString(String newString){
	//Feed a string to commander and process it - bypassing any read of the serial ports
	bufferString = newString;
	if( !isEndOfLine(bufferString.charAt( bufferString.length()-1 ) ) ) bufferString += endOfLine;//append an end of line character if none there
	disablePrompt();
	commandState.bit.commandHandled = handleCommand();
	if( ports.settings.bit.multiCommanderMode == false ) enablePrompt(); //re-enable the prompt if in single commander mode so it prints on exit
	return commandState.bit.commandHandled;
}//==============================================================================================================

void Commander::loadString(String newString){
	//Load a string to commander for processing the next time update() is called
	bufferString = newString;
	if( !isEndOfLine(bufferString.charAt( bufferString.length()-1 ) ) ) bufferString += endOfLine;//append an end of line character if none there
	commandState.bit.isCommandPending = true; 
	return;
}
//==============================================================================================================


bool Commander::endLine(){
	//add a newline to the buffer and process it - used for reading the last line of a file 
	bufferString+= endOfLine;
	commandState.bit.commandHandled = handleCommand();
	return commandState.bit.commandHandled;
}
//==============================================================================================================


void Commander::transfer(Commander& Cmdr){
	//Transfer command FROM the attached object TO this one
	//the attached ports need to be copied to this one - we assume the user has backed them up first
	ports = Cmdr.getPortSettings();
	if( ports.settings.bit.multiCommanderMode ) Cmdr.disablePrompt(); //disable the prompt for the other commander
  printCommandPrompt();
}
//==============================================================================================================

bool Commander::transferTo(const commandList_t *commands, uint32_t size, String newName){
	//Transfer command to the new command array
	attachCommands(commands, size);
	commanderName = newName;
	if( hasPayload() ){
    //Serial.println("handing payload to get command list");
		//bufferString = bufferString.substring(Cmdr.endIndexOfLastCommand+1);
		bufferString.remove(0, endIndexOfLastCommand+1);
		//Serial.print(bufferString);
		//keep this command prompt disabled if it wasn't already
		disablePrompt(); //dsiable the prompt so it doesn't print twice
		commandState.bit.commandHandled = handleCommand(); //try and handle the command
		if( ports.settings.bit.multiCommanderMode == false ) enablePrompt(); //re-enable the prompt if in single commander mode so it prints on exit
		return true;
  }
	return false;
}

void Commander::transferBack(const commandList_t *commands, uint32_t size, String newName){
	//Transfer command to the new command array
	attachCommands(commands, size);
	commanderName = newName;

	//return false;
}
//==============================================================================================================

void Commander::attachOutputPort(Stream *oPort){
	//Set the output port if you want it to be different than the input port
	ports.outPort = oPort;
}
//==============================================================================================================

void Commander::attachAltPort(Stream *aPort){
	ports.altPort = aPort;
}
//==============================================================================================================

void Commander::attachInputPort(Stream *iPort){
	ports.inPort = iPort;
}

//==============================================================================================================

void  Commander::setBuffer(uint16_t buffSize){
	bufferSize = buffSize;
	bufferString.reserve(bufferSize);
}
//==============================================================================================================


void Commander::attachCommands(const commandList_t *commands, uint32_t size){
	commandList = commands;
	//numOfCmds = sizeof(myCommands) /  sizeof(myCommands[0]); //calculate the number of commands so we know the array bounds
	commandListEntries = (uint16_t)(size / sizeof(commandList_t)); //size is the size of the whole array, not the individual entries
	computeLengths();
}
//==============================================================================================================
void Commander::quickSetHelp(){
	
	if( bufferString.indexOf("help") > -1 )	commandState.bit.quickHelp = true;
	else commandState.bit.quickHelp = false;
}

bool Commander::quickSet(String cmd, int& var){
	//look for the string, if found try and parse an int
	//print help if help was triggered
	if(qSetHelp(cmd)) return 0;
	String sub = bufferString.substring( qSetSearch(cmd)+1 );
	if(isNumber(sub)) {
		var = sub.toInt();
		return true;
	}
	return false;
}
bool Commander::quickSet(String cmd, float& var){
	//look for the string, if found try and parse an int
	//print help if help was triggered
	if(qSetHelp(cmd)) return 0;
	String sub = bufferString.substring( qSetSearch(cmd)+1 );
	if(isNumber(sub)) {
		var = sub.toFloat();
		return true;
	}
	return false;
}
bool Commander::quickSet(String cmd, double& var){
	//look for the string, if found try and parse an int
	//print help if help was triggered
	if(qSetHelp(cmd)) return 0;
	String sub = bufferString.substring( qSetSearch(cmd)+1 );
	if(isNumber(sub)) {
		//var = sub.toDouble(); //not working yet on ESP32
		var = sub.toFloat();
		return true;
	}
	return false;
}
int Commander::qSetSearch(String &cmd){
	int idx = bufferString.indexOf(cmd);
	if(idx < 0) return false;
	//idx += cmd.length();
	return idx + cmd.length();
	//if(cmd.length() == 1) idx++;
	//else 								idx = bufferString.indexOf(" ", idx+1); //find the next space
	//return idx;
}

bool Commander::qSetHelp(String &cmd){
	if(commandState.bit.quickHelp){
		print("  ");
		println(cmd);
		return 1;
	}
	return 0;
}

void  Commander::quickGet(String cmd, int var){
	//look for the string, if found try and parse an int
	//print help if help was triggered
	if(commandState.bit.quickHelp){
		print("  ");
		println(cmd);
		return;
	}
	if(bufferString.indexOf(cmd) > -1){
		print(cmd);
		print(" ");
		println(var);
	}
}
void  Commander::quickGet(String cmd, float var){
	//look for the string, if found try and parse an int
	//print help if help was triggered
	if(commandState.bit.quickHelp){
		print("  ");
		println(cmd);
		return;
	}
	if(bufferString.indexOf(cmd) > -1){
		print(cmd);
		print(" ");
		println(var);
	}
}
	
bool Commander::getInt(int &myInt){
	if(tryGet()){
		//Parse it to the variable
		String subStr = bufferString.substring(dataReadIndex);
		myInt = subStr.toInt();
		//if there is no space next, set dataReadIndex to zero and return true - you parsed an int, but next time it will fail.
		nextDelimiter();
		return true; //nextSpace();
	}else return 0;
}
bool Commander::getFloat(float &myFloat){
	if(tryGet()){
		//Parse it to the variable
		String subStr = bufferString.substring(dataReadIndex);
		myFloat = subStr.toFloat();
		//if there is no space next, set dataReadIndex to zero and return true - you parsed an int, but next time it will fail.
		nextDelimiter();
		return true; //nextSpace();

	}else return 0;
}
bool Commander::getDouble(double &myDouble){
	if(tryGet()){
		//Parse it to the variable
		String subStr = bufferString.substring(dataReadIndex);
		myDouble = subStr.toFloat();//subStr.toDouble(); //toDouble() not working on ESP32
		nextDelimiter();
		return true; //nextSpace();
	}else return 0;
}
//Try to find the next numeral from where readIndex is
bool Commander::tryGet(){
	if(dataReadIndex < endIndexOfLastCommand){
		//println("tryget found no payload");
		return 0; //nothing to see here
	}
	int indx = findNumeral(dataReadIndex); //find the next valid number string start
	
	//print("findNumeral returned ");
	//println(indx);
	if(indx < 0){ //return if it doesn't exist
		//println("returning false ");
		dataReadIndex = 0;
		return 0;
	}
	dataReadIndex = indx;
	//println("returning true");
	return 1;
}
//find the next space, set readIndex to it
bool Commander::nextSpace(){
	int indx = bufferString.indexOf(" ", dataReadIndex);
	if(indx > 0) {
		//if it exists set the read index to it for next time
		dataReadIndex = (uint16_t)indx;
		return 1;
	}else dataReadIndex = 0;
	return 0;
}
//find the next delimiter, set readIndex to it
//Delimiter is anything except - . and any numeral 0-9
bool Commander::nextDelimiter(){
	for(uint16_t n = dataReadIndex; n < bufferSize; n++){
		if(isDelimiter(bufferString.charAt(n))) {
			//print("Next delimiter found at ");
			//println(n);
			dataReadIndex = n;
			return 1;
		}
	}
	dataReadIndex = 0;
	return 0;
}
//==============================================================================================================
void Commander::printCommandPrompt(){
	if(!ports.settings.bit.commandPromptEnabled) return;
		print(commanderName);
		print(promptCharacter);
}
//==============================================================================================================
bool Commander::containsTrue(){
	if(bufferString.indexOf(" true") != -1) return true;
	return false;
}
//==============================================================================================================
bool Commander::containsOn(){
	if(bufferString.indexOf(" on") != -1) return true;
	return false;
}

//==============================================================================================================


void Commander::computeLengths(){
	//compute the length of each command
	if(commandListEntries == 0) return;
	//delete the current array of not NULL
	if(commandLengths) delete [] commandLengths;
	commandLengths = new uint8_t[commandListEntries];
	for(int n = 0; n < commandListEntries; n++){
		commandLengths[n] = getLength(n);
		if(commandLengths[n] > longestCommand) longestCommand = commandLengths[n];
	}
}

//==============================================================================================================


uint8_t Commander::getLength(uint8_t indx){
	uint8_t length = 0;
	for(uint8_t n = 0; n < 128; n++){
		if(commandList[indx].commandString[n] != NULL) length++;
		else return length;
	}
}
//==============================================================================================================
bool Commander::handleCommand(){
	if(ports.settings.bit.locked && ports.settings.bit.useHardLock){
		//if the command string starts with unlock then handle unlocking
		//println("Trying to unlock");
		tryUnlock();
		resetBuffer();
		if(!ports.settings.bit.locked){
			
			println(unlockMessage);
			printCommandPrompt();
		}
		return 0;
	}
	if(ports.settings.bit.commandPromptEnabled && !ports.settings.bit.echoTerminal) write('\n'); //write a newline if the command prompt is enabled so reply messages appear on a new line
  
	if(commandState.bit.autoFormat) startFormatting();
	/*Match command will handle internal commands 
	*/
	commandVal = matchCommand();
  bool returnVal = false;
	if(commandVal == COMMENT_COMMAND) {
		handleComment();
    returnVal = 0; //do nothing - comment lines are ignored
  }else if(commandVal == INTERNAL_COMMAND){
		//Comment or internal comnand, nothing to see here, move along.
    returnVal = 0;
	}else  if(commandVal == UNKNOWN_COMMAND) {
    //Unknown command
		
		unknownCommand();
    returnVal = 0; //unknown command function
  }
	else if(commandVal == CUSTOM_COMMAND && ports.settings.bit.locked == false){
		  returnVal = handleCustomCommand();
			if(returnVal == 1) unknownCommand();
	}
	
  else if(ports.settings.bit.locked == false){
		endIndexOfLastCommand = commandLengths[commandVal];
		dataReadIndex = endIndexOfLastCommand;
		//call the appropriate function from the function list and return the result
		if(commandVal < commandListEntries) returnVal = commandList[commandVal].handler(*this);
		//else returnVal = altCommandList[commandVal].handler(*this);
  }

  resetBuffer();
	//ports.settings.bit.commandPromptEnabled ? println("prompt on") : println("prompt off");
	printCommandPrompt();
  return returnVal;
}

void Commander::tryUnlock(){
	//try and unlock commander
	if(bufferString.indexOf('U') == 0){
		//check passphrase:
		//println("Checking passphrase");
		if(passPhrase == NULL || checkPass()) unlock();
	}
}

bool Commander::checkPass(){
	if(bufferString.indexOf(*passPhrase))	return true;
	return false;
}

void Commander::handleComment(){
	//if comments are to be printed then print out the buffer
	if(ports.settings.bit.printComments){
		print(bufferString);
	}
}
//==============================================================================================================
bool  Commander::processBuffer(int dataByte){
  if(dataByte == -1) return false; //no actual data to process

  switch(commandState.bit.bufferState){
    case BUFFER_WAITING_FOR_START:
      //DEBUG.println("Waiting for Start");
			//check for the reload character, if found print the last command to the outport and re-process the buffer
			if(dataByte == reloadCommandChar){
				//tab
				commandState.bit.newLine = true;
				commandState.bit.bufferState = BUFFER_PACKET_RECEIVED;
				ports.outPort->print(bufferString); //print the old buffer
				return true;
			}
      if( isCommandStart(dataByte) ) {
        commandState.bit.bufferState = BUFFER_BUFFERING_PACKET;
				bufferString = "";//clear the buffer
        writeToBuffer(dataByte);
        if(commandState.bit.bufferFull) resetBuffer();
      }
      break;
    case BUFFER_BUFFERING_PACKET:
      //DEBUG.println("Buffering");
      writeToBuffer(dataByte);
      if(commandState.bit.bufferFull) resetBuffer();//dump the buffer
      if(commandState.bit.newLine == true) {
        commandState.bit.bufferState = BUFFER_PACKET_RECEIVED;
				//if(bufferString.charAt(bytesWritten - 2) == '\r') bufferString.charAt(bytesWritten - 2) = '\n';
				return true; //return true because we have a newline
        //DEBUG.println("Newline Detected");
      } //unpack the data
      break;
    case BUFFER_PACKET_RECEIVED:
      //DEBUG.println("Packet received ...?");
      //if you get here then the packet has not been unpacked and more data is coming in. It may be a stray newline ... do nothing
      break;  
  }
	return false;
}
//==============================================================================================================

void Commander::writeToBuffer(int dataByte){
	if(bytesWritten == bufferSize-1){
    commandState.bit.bufferFull = true; //buffer is full
		if(ports.settings.bit.errorMessagesEnabled) println("ERR: Buffer Overflow");
    return;
  }
  //buf[bytesWritten] = dataByte;
  if(ports.settings.bit.stripCR && dataByte != '\r') bufferString += (char)dataByte; //ingore CR
	else bufferString += (char)dataByte;
  if(dataByte == endOfLine) commandState.bit.newLine = true;
  bytesWritten++;
}
//==============================================================================================================

void Commander::resetBuffer(){
	bytesWritten = 0;
  //commandState.bit.newData = false;
  commandState.bit.newLine = false;
  commandState.bit.bufferFull = false;
	commandState.bit.bufferState =  BUFFER_WAITING_FOR_START;
	
	//#if defined (CMD_ENABLE_FORMATTING)
	//if(!commandState.bit.autoFormat){
		commandState.bit.prefixMessage = false;
		commandState.bit.postfixMessage = false;
	//}
	////#endif
	commandState.bit.newlinePrinted = true;
	
  //parseState = WAITING_FOR_START;
  /*if(commandState.bit.isCommandPending){
    bufferString = pendingCommandString;
    commandState.bit.isCommandPending = false;
    pendingCommandString = "";
    
  }*/
	//else bufferString = "";
}
//==============================================================================================================
//return the index of the command, or handle the internal commands
int Commander::matchCommand(){
  //loop through the command list and see if it appears in the String
	int indexOfLongest = -1;
	uint8_t lastLength = 0;
	//first check comment char
	if(bufferString.charAt(0) == commentChar) return COMMENT_COMMAND;
	
	
  for(uint8_t n = 0; n < commandListEntries; n++){
		//check each line for a match,
		//if you get a match, check to see if the command is longer than the last stored length
		//if it is then update the last length and store the index
		//When you get a match, make sure it is followed by a space or newline - otherwise a command like 'st' will be triggered by any string that starts with 'st'
		if( commandLengths[n] > lastLength && checkCommand(n) ) {
			lastLength = commandLengths[n];
			indexOfLongest = (int)n;
		}
  }
	if(indexOfLongest > -1){
		return (int)indexOfLongest;
	}

	//Now check any default commands - these can be overridden if found in the users command list
	//First see if it starts with an int - if so then use the number function
	//Check if it is a number or minus sign
	if( isNumber(bufferString) ) return CUSTOM_COMMAND;
	for(uint16_t n = 0; n < INTERNAL_COMMAND_ITEMS; n++){
		//String intCmdLine = 
		if(bufferString.startsWith( internalCommands[n] )){
			//call the internal command function
			return handleInternalCommand(n);
		}
	}
	
  return UNKNOWN_COMMAND;
}
//==============================================================================================================

//return true if the command is valid
bool Commander::checkCommand(uint16_t cmdIdx){
	//see if the incoming command matches the command in the array index cmdIdx
	if(bufferString.startsWith( commandList[cmdIdx].commandString ) == false) return false; //no match
	if( bufferString.charAt( commandLengths[cmdIdx] ) == ' ' ) return true; //space after command
	if( bufferString.charAt( commandLengths[cmdIdx]-1 ) == ' ' ) return true; //command includes a trailing space
	if( isEndOfLine( bufferString.charAt( commandLengths[cmdIdx] ) ) ) return true; ////end of line after command
	if(bufferString.charAt( commandLengths[cmdIdx] ) == eocCharacter) return true; //alternative end of command character detected (e.g the '=' char)
	return false; //failed check
}
/*
//return true if the command is valid
bool Commander::checkAltCommand(uint16_t cmdIdx){
	//see if the incoming command matches the command in the array index cmdIdx
	if(bufferString.startsWith( altCommandList[cmdIdx].commandString ) == false) return false; //no match
	if( bufferString.charAt( altCommandLengths[cmdIdx] ) == ' ' ) return true; //space after command
	if( bufferString.charAt( altCommandLengths[cmdIdx]-1 ) == ' ' ) return true; //command includes a trailing space
	if( isEndOfLine( bufferString.charAt( altCommandLengths[cmdIdx] ) ) ) return true; ////end of line after command
	return false; //failed check
}*/
//==============================================================================================================


int Commander::handleInternalCommand(uint16_t internalCommandIndex){
	switch(internalCommandIndex){
		case 0: //help
			unlock();
			println(unlockMessage);
			//Lock Command printCommandList();
			break;
		case 1: //?
		  lock();
			println(lockMessage);
			//Unlock Command printCommanderVersion();
			break;
		case 2: //help
			if( ports.settings.bit.helpEnabled ) printCommandList();
			break;
		case 3: //?
			if( ports.settings.bit.helpEnabled ) printCommanderVersion();
			break;
		case 4: //CMDR echo 
			ports.settings.bit.echoTerminal = containsOn();
			print(F("Echo Terminal "));
			ports.settings.bit.echoTerminal ? println("on") : println("off");
			break;
		case 5: //CMDR echo alt 
			ports.settings.bit.echoToAlt = containsOn();
			print(F("Echo Alt "));
			ports.settings.bit.echoToAlt ? println("on") : println("off");
			break;
		case 6: //CMDR enable commander
			ports.settings.bit.commandParserEnabled = containsOn();
			print(F("Commander Enabled "));
			ports.settings.bit.commandParserEnabled ? println("on") : println("off");
			break;
		case 7: //CMDR enable error messages
			ports.settings.bit.errorMessagesEnabled = containsOn();
			print(F("Error Messages Enabled "));
			ports.settings.bit.errorMessagesEnabled ? println("on") : println("off");
			break;
	}
	return INTERNAL_COMMAND;
}
//==============================================================================================================

bool  Commander::handleCustomCommand(){
	//if the function pointer is NULL then return
	//If not then call the function
	if(customHandler == NULL) return 1;
	return customHandler(*this);
}
//==============================================================================================================
void Commander::unknownCommand(){
	if(ports.settings.bit.errorMessagesEnabled){
		print(F("Command: \'"));
		print(bufferString.substring(0, bufferString.length()-1));
		println(F("\' not recognised"));
	}
}
//==============================================================================================================
int Commander::findNumeral(uint8_t startIdx){
	//return the index of the start of a number string
	//print("FindNumeral started from ");
	//println(startIdx);
	for(uint8_t n = startIdx; n < bufferSize; n++){
		if(bufferString.charAt(n) == endOfLine){
			//println("found EOL");
			return -1;
		}
		if( isNumeral( bufferString.charAt(n) ) ){
			//print("IsNumeral sent ");
			//print(bufferString.charAt(n));
			//print(" from buffer index ");
			//print(n);
			//print(" isNumeral returned true, returning index = ");
			//println(n);
			return n;
		}
		if( bufferString.charAt(n) == 45 && isNumeral( bufferString.charAt(n+1) ) ) {
			//print("isNumeral returned true, after a minus sign, returning index = ");
			//println(n);
			return n;
		}
	}
	return -1;
}
bool Commander::isNumber(String str){
	//returns true if the first character is a valid number, or a minus sign followed by a number
	if( isNumeral( str.charAt(0) ) ) return true;
	if(str.charAt(0) == 45 && isNumeral( str.charAt(1) )) return true;
	return false;
}
bool Commander::isDelimiter(char ch){
	//returns true if the first character is NOT valid number, minus sign, dot or NL/CR
	if( isNumeral( ch ) || ch == '.' || ch == '-' || ch == '\n' || ch == '\r' || ch == 0) return false;
	return true;
}
//==============================================================================================================
bool Commander::isNumeral(char ch){
	if(ch > 47 && ch < 58) return true;
	return false;
}
//==============================================================================================================
bool Commander::isCommandChar(char dataByte){
  //Command chars are letters and a few other characters
  //DEBUG.print("Databyte is ");
  //DEBUG.println(dataByte);
  if(dataByte < 63 || dataByte > 126) return false;
  return true;
}
//==============================================================================================================
bool Commander::isCommandStart(char dataByte){
  //The start of the command must be a typeable character
  if( dataByte > 31 &&  dataByte < 127) return true;
  return false;
}

//==============================================================================================================

bool Commander::isEndOfLine(char dataByte){
  if(dataByte == 13 || dataByte == 10) return true;
  return false;
}

//==============================================================================================================


void Commander::printCommandList(){
	  //Prints all the commands
  uint8_t n = 0;
  //int length1 = 0;
	String cmdLine = " ";
	cmdLine.concat(commanderName);
	cmdLine.concat(F(" User Commands:"));
	println(cmdLine);
  for(n = 0; n < commandListEntries; n++){
		
		cmdLine = '\t';
		cmdLine.concat(commandList[n].commandString);
		//cmdLine += ' ';
		cmdLine.concat(getWhiteSpace(longestCommand - commandLengths[n]));
		cmdLine.concat("| ");
		cmdLine.concat(commandList[n].manualString);
		println(cmdLine);
  }
  println(F(" Internal Commands:"));
	for(n = 0; n < INTERNAL_COMMAND_ITEMS; n++){
		write('\t');
    if(n > 3){
			print(internalCommands[n]);
			println(F(" (on/off)"));
		}
		else println(internalCommands[n]);
  }
	print(F(" Reload character: "));
	println(String(reloadCommandChar));
	
	
	print(F(" Comment character: "));
	println(String(commentChar));
	
  //return 0;
}

//==============================================================================================================

String Commander::getWhiteSpace(uint8_t spaces){
	String wSpace = " ";
	for(uint8_t n = 0; n < spaces; n++){
		wSpace.concat(' ');
		//write(' ');
	}
	return wSpace;
}
//==============================================================================================================

void Commander::printCommanderVersion(){
	
	print(F("\tCommander version "));
	print(majorVersion);
	print(".");
	print(minorVersion);
	print(".");
	println(subVersion);
	
	print(F("\tEcho terminal: "));
	ports.settings.bit.echoTerminal ? println("On") : println("Off");
	
	print(F("\tEcho to Alt: "));
	ports.settings.bit.echoToAlt ? println("On") : println("Off");
	
	print(F("\tAuto Format: "));
	commandState.bit.autoFormat ? println("On") : println("Off");
	
	print(F("\tError messages: "));
	ports.settings.bit.errorMessagesEnabled ? println("On") : println("Off");
	
	print(F("\tIn Port: "));
	ports.inPort ? println("OK") : println("NULL");
	
	print(F("\tOut Port: "));
	ports.outPort ? println("OK") : println("NULL");
	
	print(F("\tAlt Port: "));
	ports.altPort ? println("OK") : println("NULL");
	
	print(F("\tLocked: "));
	ports.settings.bit.locked ? println("Yes") : println("No");
	print(F("\tLock: "));
	ports.settings.bit.useHardLock ? println("Hard") : println("Soft");
	if(customHandler != NULL)	println( F("\tCustom Cmd OK"));
}
//==============================================================================================================
