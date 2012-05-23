% Demonstrate identities as used by the request-reply pattern. Run this
% program by itself.

functor
import
    ZeroMQ at '../ZeroMQ.ozf'
    System
    Application

define
    Context = {ZeroMQ.init}

    Sink = {Context bind(router('inproc://example') $)}

    Anonymous  Identified


    proc {Dump Socket}
        RawMessages = {Socket recvMulti($)}
    in
        {System.showInfo '----------------------------------------'}
        {ForAll {Map RawMessages ByteString.toString} proc {$ Message}
            IsText = {All Message fun {$ C} C >= 32 andthen C =< 127 end}
            ShowMethod = if IsText then showInfo else show end
        in
            {System.printInfo '['#{Length Message}#'] '}
            {System.ShowMethod Message}
        end}
    end

in
    % First allow 0MQ to set the identity
    Anonymous = {Context connect(req('inproc://example') $)}
    {Anonymous send('ROUTER uses a generated UUID')}
    {Dump Sink}

    % Then set the identity ourself
    Identified = {Context connect(req('inproc://example' identity:'Hello') $)}
    {Identified send('ROUTER socket uses REQ\'s socket identity')}
    {Dump Sink}

    {Sink close}
    {Anonymous close}
    {Identified close}
    {Context close}
    {Application.exit 0}
end

