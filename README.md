# Commander is a system for handling commands sent over serial ports or other Stream objects.

## Its designed to make it easy to create complex and powerful text based interfaces for controlling your sketch.

It allows you to define a list of text commands, a function to handle each command, and some help text that can be displayed when the 'help' command is sent. All the work of reading the incoming stream data, identifying the appropriate function and calling the handler is done by the commander object. It will run on most Arduino boards but is more suited to devices with large memory.

Commander is attached to Stream object so it can be used with Serial ports, files on a SD cards, or Bluetooth Serial and other Stream based objects on wireless capable Arduinos such as the ESP32.

Commander can have up to three Stream objects connected at once, an input stream, output stream and auxiliary stream. As well as reading commands and passing them to the handler functions Commander can route or copy data to another port, echo data back to you and redirect or copy responses to a different port. When using SDFat, Commanders input Stream can be attached to one file to read commands, the output Stream can be attached to a second file for logging any responses generated by the command handler, and the aux stream can copy all those responses to a serial port.

Commander is designed so that the list of commands, handlers and help text are separate from the Commander object, this allows command lists to be shared between several instances of a Commander object, for example where the same command set needs to be available via USB and Bluetooth Serial. It also allows different command lists to be dynamically loaded so multiple command lists, and multiple Commander objects can be combined to produce hierarchical command structures.

Commands can be chained together in a single line and Commander incorporates a set of functions for extracting any payload that comes after a command and for parsing variables in the payload. It can also augment responses sent to its output and auxiliary Streams by adding prefix and postfix text, for example each line can be enclosed with opening and closing html tags, or prefixed with a command.

Built in commands like Help will generate a help page that lists all the commands and any help text. Additional built in commands can be used to toggle error reporting and port echoing and all built in commands can be overridden by the user with their own handler. Lock and Unlock commands can be used to impliment a password system with two levels. A soft lock will allow internal commands to be used (including help) but will not run any user commands. A hard lock will block all commands except unlock. An optional password can be used and is stored outside the Commander object in the users sketch.

Commander can use an optional command prompt with user defined text to emulate the feel of a command line, this prompt can be changed dynamically to indicate the current context, for example it can show the working directory of a file system, or the title of a sub command list. Commander also has a user defined 'reload' character that will reload the last command. By default this is / so, for example, if you sent a command called 'print sensors' and then want to send the same command again, you just need to type / to repeat it. A user defined 'comment' character (# by default) can be put in front of a line to tell Commander to ignore it. This can be handy when reading SD card files if you want to put comments into the file.

### The following list of examples demonstrate various ways to use Commander

__BasicCommands:__ Demonstrates setting and getting integer and float values with a command list and setting multiple values with a single command.

__QuickSet:__ Demonstrates an in built method for packing some commands in a single command handler for faster coding whilst retaining the help system for listing commands.

__ESP32-SerialBTCommands:__ Uses a BluetoothSerial object so commands can be sent vial Bluetooth.

__FileRead:__ Open an SD card file that contains a set of commands and read the contents into Commander. Responses to commands are fed back to a Serial port.

__FileReadLog:__ Open an SD card file that contains a set of commands and read the contents into Commander. Responses to commands are written to another file and copied to a Serial port.

__FileNavigation:__ Used SDFat and a set of commands for listing files, navigating and creating directories, renaming and deleting files and directories and printing out files.

__FormattedReplies:__ Shows how to use the pre and postfix formatting, and command chaining so formatting for another command can be invoked.

__SimpleMultiLayer:__ Shows how three command lists can be used with one Commander object to create a multi-level command structure. This example has sub commands for setting variable, and more for reading back variables. These commands can be invoked from the top level (e.g 'get int') or the sub level can be invoked ('get') and then commands from that level invoked directly ('int') before an 'exit' command returns you to the top level. The help command can be used to get help for every level.

__FullMultiLayer:__ This example behaves in an almost identical way to SimpleMultiLayer but uses three Commander objects. Navigating between different levels is handled by passing control from one Commander object to another rather than loading different command lists into the same object.

__PrefabFileExplorer:__ Demonstrates the use of a prefabricated command structure (in PrefabFileNavigator.h) to create a sub menu for navigating and manipulating files on an SD card. The prefab allows files to be created and written to but a suitable terminal application needs to be used - The terminal application needs to be able to send the ASCII value 4 in order to terminate the file download and return control to the command system. The Arduino serial terminal will not allow this so we do not recommend using it with the 'write' command.

__NumberCommand:__ (To Be Done!) Demonstrates a special class of command for handling numbers. It is designed to allow data files to be uploaded and unpacked into an array.

__TelnetCommand:__ (To Be Done) Interface a Telnet session to Commander so that commands can be accessed remotely via WiFi.

__htmlCommand:__ (To Be Done) Feed HTML page requests to Commander and generate HTML formatted responses in reply.

### How it works (roughly speaking)

The command list is an array of structures and each element contains the command string, a function pointer, and a help text string. These are all defined before the code is compiles, rather than being assigned dynamically when your code starts in order to reduce the amount of dynamic memory allocation and ensure the sketch starts quickly, particularly if using very large command sets. When you load a command list into a Commander object it scans the list and records the lengths of all the commands - this is used as part of the command matching algorithm.

When Commander reads a Stream or is fed a String it places it in a buffer and tries to match the start of the string to a command (unless it was rejected as a comment or the reload character was detected). If a command match is found it invokes the users command handler function and waits for it to finish. The buffer is a String object and is public so it can be read and manipulated by the users code from their handler function, and all the Arduino String methods can be used with it.

If it can't find a match it looks for a built in command and will execute the handler if a match is found. When Commander is finished it will check to see if the command prompt is enabled and if so, it will print out the prompt on a new line.

Because Commander checks the user commands first you can override any of the built in commands with your own.

There are a full set of Stream print and write functions that can be used, and they ensure that printed responses will be routed to the Commander objects specified output port, and to the aux port if enabled, and they ensure that any pre or postfix formatting is applied.

The command match system relies on each comment ending with either a newline or a space, or a special user defined character. If the command doesn't have any arguments it will normally end in a newline but if it has any arguments then they must be separated by a space, or the user defined ‘eocCharacter’ (which is ’=’ by default) - The eocCharacter allows you use key=value properties commands like this: 'myvariable=3' where myvariable is the command and 3 is the argument.

### Basic code structure

To create a command system the user needs to create the command list array, and all the command function handlers. A command list array will look something like this (This is all taken from the BasicCommands example):

```
const commandList_t masterCommands[] = {
  {"hello",      helloHandler,    "hello"},
  {"get int",    getIntHandler,   "get an int"},
  {"set int",    setIntHandler,   "set an int"},
  {"get float",  getFloatHandler, "get a float"},
  {"set float",  setFloatHandler, "set a float"},
  {"myint",      setIntHandler,   "try myint=23"},
  {"myfloat",    setFloatHandler, "try myfloat=23.5"},
  {"set ints",   getIntsHandler,   "set up to four ints"},
  {"set floats",   getFloatsHandler,   "set up to four floats"},
};
```
Each line specifies one command (and is one element in the command array). The first text string is the actual command, the second is the name of the function that will handle the command and the third string is the help text that will print out when you type help.

To add a command simply copy and paste in a new line, edit the text and create a command handler that matches the template below.

The command handlers need to follow the same template. Each must return a boolean value, and take a Commander object as an argument - When the Commander object calls the function it will pass a reference to its self to the function so the users code can access that Commander object and all its methods and variables.

The function template looks like this:

```
bool myFunc(Commander &Cmdr){
  //put your command handler code here
  return 0;
}
```
The commander object needs to be declared, then initialised at the start of your sketch, and it needs to be given a command list, a Stream object to use and it needs to know how big the Command list is.

```
Commander cmd;
const uint16_t numOfMasterCmds = sizeof(masterCommands);
void setup() {
  Serial.begin(115200);
  cmd.begin(&Serial, masterCommands, numOfMasterCmds);
}
```
Here we are attaching _Serial_ to Commander, and giving it the _masterCommands_ command list and the _numOfMasterCmds_ variable which tells Commander how big the command array is (so it knows how many commands there are).

_numOfMasterCmds_ has been initialised already with the size of the array.

__Important note: If you want to place the Command array after the setup() function in your sketch, for example in a different tab, _numOfMasterCmds_ needs to be initialised after the command list array, but the compiler needs to know about it before it reaches the setup() in your code.__

To do this you need to _forward declare_ the command list array and the _numOfMasterCmds_ variable before the setup() function like so:


```
extern const uint16_t numOfMasterCmds;
extern const commandList_t masterCommands[];
```

This forward declaration tells the compiler that these variables exist, but have been initialised (given a starting value) somewhere else in the code.

__Command Handler Functions__

When you write your command handler you can access the Commanders methods and the command buffer using the Cmdr reference.

In this example the command handler simply used the Cmdr objects print methods to reply with a message that includes the contents of the buffer.

```
bool helloHandler(Commander &Cmdr){
  Cmdr.print("Hello! this is ");
  Cmdr.println(Cmdr.commanderName);
  Cmdr.print("This is my buffer: ");
  Cmdr.print(Cmdr.bufferString);
  return 0;
}
```

Commander has a built in method of parsing integer and float values, this can be used to extract numeric values from a commands payload.

```
bool setIntHandler(Commander &Cmdr){
  if(Cmdr.getInt(myInt)){
    Cmdr.print("myInt set to ");
    Cmdr.println(myInt);
  }
  return 0;
}
```
The method Cmdr.getInt(myInt) checks to see if it can find the start of a number in the payload (the part of the command buffer after the actual command) If it finds one then it converts it into an int and assigns it to the variable referenced in the function call - in this case _myInt_ - The function will return a boolean value when it finishes, this will be TRUE if the attempt was successful, and false if it was not (if your variable was not updated).

The _getInt()_ and _getFloat()_ methods keep track of where they are in the buffer so you can use them to extract a series of numbers with one command. The following code shows how to unpack up to four ints into an array. If you include less than four ints after the command, it will unpack the ones you did send, and if you include too many it will unpack only the first four.

```
bool getIntsHandler(Commander &Cmdr){
  //create an array to store any values we find
  int values[4] = {0,0,0,0};
  for(int n = 0; n < 4; n++){
    //try and unpack an int, if it fails there are no more left so exit the loop
    if(Cmdr.getInt(values[n])){
      Cmdr.print("unpacked ");
      Cmdr.println(values[n]);
    }else break;
  }
  //print it out
  Cmdr.println("Array contents after processing:");
  for(int n = 0; n < 4; n++){
    Cmdr.print(n);
    Cmdr.print(" = ");
    Cmdr.println(values[n]);
  }
  return 0;
}
```
In the example we are using the command _set ints_ which has been defined in the command array. Sending the command string 'set ints 12 34 56 78' will produce the following output on the serial port:

> unpacked 12

> unpacked 34

> unpacked 56

> unpacked 78

> Array contents after processing:

> 0 = 12

> 1 = 34

> 2 = 56

> 3 = 78


We can use commas instead of spaces in the command string so the command 'set ints 12,34,56,78' will produce exactly the same result.

_Disclaimer: I'm not the best software engineer in the world so there may be some bits of silliness in my code. I welcome contributions that will improve Commander so long as they maintain a good balance between features and efficiency._

Written by Bill Bigge.
MIT license, all text above must be included in any redistribution
