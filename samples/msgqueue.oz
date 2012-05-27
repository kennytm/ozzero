% Simple message queuing broker
% Same as request-reply broker but using QUEUE device

functor
import
    ZeroMQ at '../ZeroMQ.ozf'
    Application

define
    Context = {ZeroMQ.init}

    % Socket facing clients
    Frontend = {Context.bind router('tcp://*:5559')}

    % Socket facing services
    Backend = {Context.bind dealer('tcp://*:5560')}
in
    % Start built-in device
    {ZeroMQ.device queue Frontend Backend}

    % We never get here...
    {Frontend.close}
    {Backend.close}
    {Context.close}
    {Application.exit 0}
end

