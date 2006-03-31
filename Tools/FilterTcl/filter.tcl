proc AgentCreatedCallback {id userData agent} {
	puts "Agent_callback:: [$agent GetAgentName] created"
}

# This is a lazy function which will either return the interpreter
# already associated with this agent or create a new one for this agent
proc getInterpreter {agentName} {
	# See if we already have an interpreter for this agent
	#puts "Looking for interpreter for $agentName"
	global interpreter

	foreach i [interp slaves] {
		#puts "Have interpreter $i"
		if [string equal $i $agentName] {
			#puts "Found match for $i"
			return $i
		}
	}
	
	puts "No interpreter matched so creating one"
	set interpreter [interp create $agentName]

	# We need to source the overloaded commands into the child interpreter
	# so they'll be correctly in scope within that interpreter.
	# Also, right now we're executing inside SoarLibrary\bin so we need
	# play this game to find the file to load.
	$interpreter eval source ../../Tools/FilterTcl/commands.tcl
	
	return $interpreter
}

proc MyFilter {id userData agent filterName commandLine} {
	set name [$agent GetAgentName]
	set interpreter [getInterpreter $name]

	puts "$name Command line $commandLine"

	# It seems if we get an error here that almost seems to "throw an exception"
	# and we leave execution immediately and report the failure back to Soar directly.
	# What we want is to catch that error and control the response.
	set result [$interpreter eval $commandLine]	

	puts "Result is $result"
		
	# For the moment, always return "print s1" as the result of the filter
	return "print s1"
}

proc createFilter {} {
    global smlEVENT_AFTER_AGENT_CREATED smlEVENT_BEFORE_AGENT_REINITIALIZED 
    global _kernel
    global sml_Names_kFilterName
    
	# Then create a kernel
	#set _kernel [Kernel_CreateKernelInCurrentThread SoarKernelSML 0]

	set _kernel [Kernel_CreateRemoteConnection]

	if {[$_kernel HadError]} {
		puts "Error creating kernel: "
		puts "[$_kernel GetLastErrorDescription]\n"
		return
	}

	puts "Created kernel\n"

	# Practice callbacks by registering for agent creation event
	set agentCallbackId0 [$_kernel RegisterForAgentEvent $smlEVENT_AFTER_AGENT_CREATED AgentCreatedCallback ""]
	
	# Register the filter callback
	set filterCallbackId0 [$_kernel RegisterForClientMessageEvent $sml_Names_kFilterName MyFilter ""]
	
	# Execute a couple of simulated commands, so it's easier to see errors and problems
	set agent [$_kernel GetAgent soar1]
	set res1 [MyFilter 123 "" $agent $sml_Names_kFilterName "print s3"]
	set res2 [MyFilter 123 "" $agent $sml_Names_kFilterName "print s4"]
	set res3 [MyFilter 123 "" $agent $sml_Names_kFilterName "puts hello"]
	set res4 [MyFilter 123 "" $agent $sml_Names_kFilterName "pwd"]
	set res5 [MyFilter 123 "" $agent $sml_Names_kFilterName "set test hello"]
	set res6 [MyFilter 123 "" $agent $sml_Names_kFilterName "puts \$test"]
}

# Start by loading the tcl interface library
# It seems this has to happen outside of the proc or I get a failure
set soar_library [file join [pwd]]
lappend auto_path  $soar_library

package require tcl_sml_clientinterface

createFilter
