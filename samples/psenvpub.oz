% Pubsub envelope publisher

functor
import
    ZeroMQ at '../ZeroMQ.ozf'
    Application

define
    % Prepare our context and publisher
    Context = {ZeroMQ.init}
    Publisher = {Context bindSocket(pub 'tcp://*:5563' $)}

    proc {PublisherLoop}
        % Write two messages, each with an envelope and content
        {Publisher sendMulti(['A' "We don't want to see this"])}
        {Publisher sendMulti(['B' "We would like to see this"])}
        {Delay 1000}
        {PublisherLoop}
    end

in
    {PublisherLoop}

    % We never get here but clean up anyhow
    {Publisher close}
    {Context close}
    {Application.exit 0}
end

