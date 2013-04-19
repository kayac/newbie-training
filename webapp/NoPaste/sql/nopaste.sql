CREATE TABLE IF NOT EXISTS `users` (
  `id` INTEGER NOT NULL auto_increment PRIMARY KEY,
  `username` varchar(255) NOT NULL,
  `password` varchar(255) NOT NULL,
  UNIQUE KEY `username_uniq_idx` (`username`)
) CHARACTER SET utf8 ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS `posts` (
  `id`      INTEGER NOT NULL auto_increment PRIMARY KEY,
  `user_id` INTEGER NOT NULL,
  `content` TEXT,
  `created_at` TIMESTAMP NOT NULL
) CHARACTER SET utf8 ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS `stars` (
  `id`      INTEGER NOT NULL auto_increment PRIMARY KEY,
  `user_id` INTEGER NOT NULL,
  `post_id` INTEGER NOT NULL
) CHARACTER SET utf8 ENGINE=InnoDB;

