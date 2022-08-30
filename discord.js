const { Client, Collection, GatewayIntentBits, Routes, REST, InteractionType, ActivityType } = require('discord.js');
const { discord: { token, client_id, channels }, auto_verify } = require('./config.json');
const fs = require('node:fs');
const needle = require('needle');
const replayTools = require('./replayTools');
const twitch = require('./twitch');

const client = new Client({
  "intents": [
    GatewayIntentBits.Guilds,
    GatewayIntentBits.GuildMembers,
    GatewayIntentBits.GuildMessages
  ]
});

const commands = [ ];
client.commands = new Collection();
const commandFiles = fs.readdirSync('./commands').filter(file => file.endsWith('.js'));

for (const file of commandFiles) {
  const command = require(`./commands/${file}`);
  client.commands.set(command.data.name, command);
  commands.push(command.data.toJSON());
}

const rest = new REST({ version: '10' }).setToken(token);

(async () => {
  try {
    await rest.put(
      Routes.applicationCommands(client_id),
      { "body": commands },
    );
  } catch (error) {
    console.error(error);
  }
})();

client.once('ready', () => {
  client.user.setPresence({
    "status": 'online',
    "activities": [{
      "type": ActivityType.Playing,
      "name": 'Dustforce'
    }]
  });
});

client.on('interactionCreate', async (interaction) => {
  if (!interaction.type === InteractionType.ApplicationCommand) return;
  const { commandName: command } = interaction;
  if (!client.commands.has(command)) return;
  if (interaction.channelId !== channels['bot'] && interaction.channelId !== channels['bot-testing']) return;
  try {
    await client.commands.get(command).execute(interaction);
  } catch (error) {
    console.error(error);
    await interaction.reply({
      content: 'There was an error while executing this command!',
      ephemeral: true
    });
  }
});

function uwu (str) {
  str = str.replace(/r|l/gi, (match) => {
    if (match === 'r' || match === 'l') return 'w';
    if (match === 'R' || match === 'L') return 'W';
  });
  if (Math.round(Math.random())) {
    str = str + ' owo';
  } else {
    str = str + ' uwu';
  }
  return str;
}

function toWeirdCase (pattern, str) {
  const length = pattern.length;
  return str.split('').map((v, i) => {
    const offset = 1;
    const character = pattern[(i % (length - offset)) + offset];
    if (character === character.toLowerCase()) {
      return v.toLowerCase();
    }
    return v.toUpperCase();
  }).join('');
}

function toStrimFormat(message) {
  return message.replace(/st(r|w)eam/i, "st$1im").replace(/st(r|w)iming/i, "st$1imming");
}

client.on('messageCreate', async (message) => {
  let streamCommandRegex = /^(\.|!)(st(r|w)(ea|i)ms)$/i;
  let stweamCommandRegex = /^(\.|!)(stw(ea|i)ms)$/i;
  let strimCommandRegex = /^(\.|!)(st(r|w)ims)$/i;
  let streamNotCased = /^(\.|!)(st(r|w)(ea|i)ms)$/;
  if ([channels['dustforce'],channels['races'],channels['tasforce'],channels['mapmaking']].indexOf(message.channel.id) > -1) {
    let noThumbnailRegex = /^(\.|!)(nt)$/i;
    replayblock: if (message.content.indexOf('dustkid.com/replay/') !== -1 && !noThumbnailRegex.test(message.content.split(/ |\n/)[0])) {
      let replay_id = Number(message.content.split('dustkid.com/replay/')[1].split(/ |\n/)[0].replace(/[^0-9\-]/g, ''));
      if (typeof replay_id === 'number' && !isNaN(replay_id)) {
        message.channel.sendTyping();
        let responseCounter = 0;
        let replay;
        try {
          replay = await needle('get', `https://dustkid.com/replayviewer.php?replay_id=${replay_id}&json=true&metaonly=true`);
          if (/^text\/html/.test(replay.headers['content-type'])) throw new Error('Replay not found.');
          replay = JSON.parse(replay.body);
        } catch (e) {
          console.error(e);
          break replayblock;
        }
        if (!replay) break replayblock;
        let tags = '';
        if (!Array.isArray(replay.tag)) {
          if (typeof replay.tag.version === 'string') {
            //tags = '\nDustmod version: ' + replay.tag.version;
          }
          if (typeof replay.tag.mode === 'string' && replay.tag.mode !== '') {
            tags = '\nMode: ' + replay.tag.mode;
          }
        }
        const usernameWrapper = '**[' + replay.username + '](https://dustkid.com/profile/' + replay.user + '/)**';
        const camera = '[<:camera:401772771908255755>](https://dustkid.com/replay/' + replay.replay_id + ')';
        let ranks = '';
        let tas = '';
        if (replay.validated === -1 && replay.tag.mode === 'dbg_0') {
          tags = '';
          tas = ' - [TAS]';
          replay.tag.mode = '';
        } else if (replay.validated === -5) {
          tas = ' - [TAS]';
        } else if (replay.rank_all_time !== false && replay.rank_all_score !== false) {
          let time_ties = '';
          let score_ties = '';
          if (replay.rank_all_score_ties > 0) {
            score_ties = ' (' + (replay.rank_all_score_ties + 1) + '-way tie)';
          }
          if (replay.rank_all_time_ties > 0) {
            time_ties = ' (' + (replay.rank_all_time_ties + 1) + '-way tie)';
          }
          ranks = 'Score Rank: ' + replayTools.rankToStr(replay.rank_all_score + 1) + score_ties + '\n' +
                  'Time Rank: '  + replayTools.rankToStr(replay.rank_all_time  + 1) + time_ties  + '\n';
        }
        if (replay.level === 'unknown' && replay.levelname === 'unknown' && replay.dustkid === 1 && replay.replay_id < 0 && replay.rank_all_time === false && replay.rank_all_time === false) {
          replay.level = 'random';
          replay.levelname = 'Dustkid Daily';
          try {
            let dailyStats = await needle('get', `https://dustkid.com/dailystats.php`);
            let dailyID = Number(dailyStats.body.split('\n')[0].split(' ')[1]) + 2;
            replay.level = 'random' + dailyID.toString();
          } catch (e) {
            console.error(e);
          }
        }
        let replayMessage = {
          "author": {
            "name": replay.levelname,
            "url": 'https://dustkid.com/level/' + encodeURIComponent(replay.level),
            "icon_url": 'https://cdn.discordapp.com/emojis/' + replayTools.characterIcons(replay.character) + '.png'
          },
          "description": camera + ' ' + usernameWrapper + tas + '\n' +
            'Score: ' + replayTools.scoreToIcon(replay.score_completion) + replayTools.scoreToIcon(replay.score_finesse) + '\n' +
            'Time: ' + replayTools.parseTime(replay.time) + '\n' + ranks +
            '<:apple:230164613424087041> ' + replay.apples + tags,
          "footer": {
            "text": 'Date'
          },
          "timestamp": new Date(Number(replay.timestamp) * 1000)
        };
        message.channel.send({"embeds":[replayMessage]}).catch(console.error);
      }
    }
  }
  if (message.channel.id === channels['holding']) {
    let holdingRole = message.member.guild.roles.cache.find((role) => role.name === 'holding');
    if (message.content.toLowerCase() === '!verify' && message.member.roles.cache.has(holdingRole.id)) {
      message.member.roles.remove(holdingRole);
      if (auto_verify.indexOf(message.member.id) === -1) auto_verify.push(message.member.id);
    }
  }
  if ([channels['dustforce'],channels['bot'],channels['bot-testing']].indexOf(message.channel.id) > -1) {
    if (streamCommandRegex.test(message.content)) {
      message.channel.sendTyping();
      let applyWeirdCase = !streamNotCased.test(message.content);
      let applyStrimFormat = strimCommandRegex.test(message.content);
      let streams = twitch.getStreams();
      let nobodyStreaming = 'Nobody is streaming.';
      let unknownStreaming = 'At least 1 person is streaming. I\'ll push notification(s) after I finish gathering data.';
      if (streams.length === 0) {
        if (stweamCommandRegex.test(message.content)) nobodyStreaming = uwu(nobodyStreaming);
        if (applyStrimFormat) nobodyStreaming = toStrimFormat(nobodyStreaming);
        if (applyWeirdCase) nobodyStreaming = toWeirdCase(message.content, nobodyStreaming);
        message.channel.send(nobodyStreaming);
      } else {
        let streamsString = '';
        for (let stream of streams) {
          let streamTitle = stream["title"];
          if (stweamCommandRegex.test(message.content)) streamTitle = uwu(streamTitle);
          if (applyStrimFormat) streamTitle = toStrimFormat(streamTitle);
          if (applyWeirdCase) streamTitle = toWeirdCase(message.content, streamTitle);
          streamTitle = streamTitle.replace(/\\(\*|@|<|>|:|_|`|~|\\)/g, '$1').replace(/(\*|@|<|>|:|_|`|~|\\)/g, '\\$1');
          streamsString += '<' + stream["url"] + '> - ' + streamTitle + '\n';
        }
        if (streamsString === '') {
          if (stweamCommandRegex.test(message.content)) unknownStreaming = uwu(unknownStreaming);
          if (applyStrimFormat) unknownStreaming = toStrimFormat(unknownStreaming);
          if (applyWeirdCase) unknownStreaming = toWeirdCase(message.content, unknownStreaming);
          message.channel.send(unknownStreaming);
        } else {
          streamsString = streamsString.slice(0, -1);
          message.channel.send(streamsString);
        }
      }
    }
  }
});

client.on('guildMemberAdd', async (member) => {
  if (member.guild.id === '83037671227658240') {
    let holdingRole = member.guild.roles.cache.find((role) => role.name === 'holding');
    if (auto_verify.indexOf(member.id) === -1) {
      let holdingChannel = member.guild.channels.cache.get(channels['holding']);
      member.roles.add(holdingRole);
      holdingChannel.send('<@' + member.id + '> type !verify to see the other channels. This is an anti-bot measure.');
    }
  }
});

client.login(token);

module.exports = client;