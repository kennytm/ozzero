% Reading from multiple sockets
% This version uses a simple recv loop

functor
import
    ZeroMQ at '../ZeroMQ.ozf'
    Application
    System

define
    % Prepare our context and sockets
    Context = {ZeroMQ.init}

    % Connect to task ventilator
    Receiver = {Context connectSocket(pull 'tcp://localhost:5557' $)}

    % Connect to weather server
    Subscriber = {Context connectSocket(sub 'tcp://localhost:5556' $)}

    % Process messages from both sockets
    % We prioritize traffic from the task ventilator
    proc {ProcessLoop}
	proc {Process Socket SocketName}
	    Message = {Socket recvDontWait($)}
	in
	    if Message \= unit then
		{System.showInfo 'Process '#SocketName#': '#Message}
		{Process Socket SocketName}
	    end
	end
    in
	% Process any waiting tasks/updates
	{Process Receiver 'task'}

	% Process any waiting weather updates
	{Process Subscriber 'weather update'}

	% No activity, so sleep for 1 msec
	{Delay 1}

	{ProcessLoop}
    end

in
    {System.showInfo 1}
    {Subscriber set(subscribe:'10001 ')}
    {System.showInfo 1}
    {ProcessLoop}

    % We never get here but clean up anyhow
    {Context close}
    {Application.exit 0}
end

